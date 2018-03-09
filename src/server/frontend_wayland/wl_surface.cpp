/*
 * Copyright © 2018 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Christopher James Halse Rogers <christopher.halse.rogers@canonical.com>
 *              William Wold <william.wold@canonical.com>
 */

#include "wl_surface.h"

#include "wayland_utils.h"
#include "wl_mir_window.h"
#include "wl_subcompositor.h"
#include "wlshmbuffer.h"

#include "generated/wayland_wrapper.h"

#include "mir/graphics/buffer_properties.h"
#include "mir/frontend/session.h"
#include "mir/compositor/buffer_stream.h"
#include "mir/executor.h"
#include "mir/graphics/wayland_allocator.h"
#include "mir/shell/surface_specification.h"

namespace mir
{
namespace frontend
{

WlSurface::WlSurface(
    wl_client* client,
    wl_resource* parent,
    uint32_t id,
    std::shared_ptr<Executor> const& executor,
    std::shared_ptr<graphics::WaylandAllocator> const& allocator)
    : Surface(client, parent, id),
        allocator{allocator},
        executor{executor},
        role{null_wl_mir_window_ptr},
        pending_buffer{nullptr},
        pending_frames{std::make_shared<std::vector<wl_resource*>>()},
        destroyed{std::make_shared<bool>(false)}
{
    auto session = get_session(client);
    graphics::BufferProperties const props{
        geometry::Size{geometry::Width{0}, geometry::Height{0}},
        mir_pixel_format_invalid,
        graphics::BufferUsage::undefined
    };

    stream_id = session->create_buffer_stream(props);
    stream = session->get_buffer_stream(stream_id);

    // wl_surface is specified to act in mailbox mode
    stream->allow_framedropping(true);
}

WlSurface::~WlSurface()
{
    *destroyed = true;
    if (auto session = get_session(client))
        session->destroy_buffer_stream(stream_id);
}

void WlSurface::set_role(WlMirWindow* role_)
{
    role = role_;
}

std::unique_ptr<WlSurface, std::function<void(WlSurface*)>> WlSurface::add_child(WlSubsurface* child)
{
    children.push_back(child);
    return std::unique_ptr<WlSurface, std::function<void(WlSurface*)>>(
        this,
        [child=child](WlSurface* self)
        {
            self->remove_child(child);
        });
}

void WlSurface::invalidate_buffer_list()
{
    role->invalidate_buffer_list();
}

void WlSurface::populate_buffer_list(std::vector<shell::StreamSpecification>& buffers) const
{
    buffers.push_back({stream_id, buffer_offset_, {}});
    for (WlSubsurface* subsurface : children)
    {
        subsurface->populate_buffer_list(buffers);
    }
}

WlSurface* WlSurface::from(wl_resource* resource)
{
    void* raw_surface = wl_resource_get_user_data(resource);
    return static_cast<WlSurface*>(static_cast<wayland::Surface*>(raw_surface));
}

void WlSurface::remove_child(WlSubsurface* child)
{
    children.erase(std::remove(children.begin(), children.end(), child), children.end());
}

void WlSurface::destroy()
{
    *destroyed = true;
    role->destroy();
    wl_resource_destroy(resource);
}

void WlSurface::attach(std::experimental::optional<wl_resource*> const& buffer, int32_t x, int32_t y)
{
    if (x != 0 || y != 0)
    {
        mir::log_warning("Client requested unimplemented non-zero attach offset. Rendering will be incorrect.");
    }

    role->visiblity(!!buffer);

    pending_buffer = buffer.value_or(nullptr);
}

void WlSurface::damage(int32_t x, int32_t y, int32_t width, int32_t height)
{
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    // This isn't essential, but could enable optimizations
}

void WlSurface::damage_buffer(int32_t x, int32_t y, int32_t width, int32_t height)
{
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    // This isn't essential, but could enable optimizations
}

void WlSurface::frame(uint32_t callback)
{
    pending_frames->emplace_back(
        wl_resource_create(client, &wl_callback_interface, 1, callback));
}

void WlSurface::set_opaque_region(std::experimental::optional<wl_resource*> const& region)
{
    (void)region;
}

void WlSurface::set_input_region(std::experimental::optional<wl_resource*> const& region)
{
    (void)region;
}

void WlSurface::commit()
{
    buffer_offset_.commit();
    if (pending_buffer)
    {
        auto send_frame_notifications =
            [executor = executor, frames = pending_frames, destroyed = destroyed]()
            {
                executor->spawn(run_unless(
                    destroyed,
                    [frames]()
                    {
                        /*
                         * There is no synchronisation required here -
                         * This is run on the WaylandExecutor, and is guaranteed to run on the
                         * wl_event_loop's thread.
                         *
                         * The only other accessors of WlSurface are also on the wl_event_loop,
                         * so this is guaranteed not to be reentrant.
                         */
                        for (auto frame : *frames)
                        {
                            wl_callback_send_done(frame, 0);
                            wl_resource_destroy(frame);
                        }
                        frames->clear();
                    }));
            };

        std::shared_ptr<graphics::Buffer> mir_buffer;

        if (wl_shm_buffer_get(pending_buffer))
        {
            mir_buffer = WlShmBuffer::mir_buffer_from_wl_buffer(
                pending_buffer,
                std::move(send_frame_notifications));
        }
        else
        {
            auto release_buffer = [executor = executor, buffer = pending_buffer, destroyed = destroyed]()
                {
                    executor->spawn(run_unless(
                        destroyed,
                        [buffer](){ wl_resource_queue_event(buffer, WL_BUFFER_RELEASE); }));
                };

            mir_buffer = allocator->buffer_from_resource(
                    pending_buffer,
                    std::move(send_frame_notifications),
                    std::move(release_buffer));
        }

        /*
         * This is technically incorrect - the resize and submit_buffer *should* be atomic,
         * but are not, so a client in the process of resizing can have buffers rendered at
         * an unexpected size.
         *
         * It should be good enough for now, though.
         *
         * TODO: Provide a mg::Buffer::logical_size() to do this properly.
         */
        stream->resize(mir_buffer->size());
        role->new_buffer_size(mir_buffer->size());
        role->commit();
        stream->submit_buffer(mir_buffer);

        pending_buffer = nullptr;
    }
    else
    {
        role->commit();
    }
}

void WlSurface::set_buffer_transform(int32_t transform)
{
    (void)transform;
    // TODO
}

void WlSurface::set_buffer_scale(int32_t scale)
{
    (void)scale;
    // TODO
}

}
}
