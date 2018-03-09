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

#ifndef MIR_FRONTEND_WL_SURFACE_H
#define MIR_FRONTEND_WL_SURFACE_H

#include "generated/wayland_wrapper.h"

#include "double_buffered.h"

#include "mir/frontend/buffer_stream_id.h"
#include "mir/frontend/surface_id.h"
#include <mir/geometry/displacement.h>

#include <vector>

namespace mir
{
class Executor;

namespace graphics
{
class WaylandAllocator;
}
namespace shell
{
struct StreamSpecification;
}

namespace frontend
{
class BufferStream;
class WlMirWindow;
class WlSubsurface;

class WlSurface : public wayland::Surface
{
public:
    WlSurface(wl_client* client,
              wl_resource* parent,
              uint32_t id,
              std::shared_ptr<mir::Executor> const& executor,
              std::shared_ptr<mir::graphics::WaylandAllocator> const& allocator);

    ~WlSurface();

    std::shared_ptr<bool> destroyed_flag() const { return destroyed; }
    geometry::Displacement const& buffer_offset() const { return buffer_offset_; }

    void set_role(WlMirWindow* role_);
    void set_buffer_offset(geometry::Displacement const& offset) { return buffer_offset_ = offset; }
    std::unique_ptr<WlSurface, std::function<void(WlSurface*)>> add_child(WlSubsurface* child);
    void invalidate_buffer_list();
    void populate_buffer_list(std::vector<shell::StreamSpecification>& buffers) const;

    mir::frontend::BufferStreamId stream_id;
    std::shared_ptr<mir::frontend::BufferStream> stream;
    mir::frontend::SurfaceId surface_id;       // ID of any associated surface

    static WlSurface* from(wl_resource* resource);

private:
    void remove_child(WlSubsurface* child);

    std::shared_ptr<mir::graphics::WaylandAllocator> const allocator;
    std::shared_ptr<mir::Executor> const executor;

    WlMirWindow* role;
    std::vector<WlSubsurface*> children;

    wl_resource* pending_buffer;
    DoubleBuffered<geometry::Displacement> buffer_offset_;
    std::shared_ptr<std::vector<wl_resource*>> const pending_frames;
    std::shared_ptr<bool> const destroyed;

    void destroy() override;
    void attach(std::experimental::optional<wl_resource*> const& buffer, int32_t x, int32_t y) override;
    void damage(int32_t x, int32_t y, int32_t width, int32_t height) override;
    void frame(uint32_t callback) override;
    void set_opaque_region(std::experimental::optional<wl_resource*> const& region) override;
    void set_input_region(std::experimental::optional<wl_resource*> const& region) override;
    void commit() override;
    void damage_buffer(int32_t x, int32_t y, int32_t width, int32_t height) override;
    void set_buffer_transform(int32_t transform) override;
    void set_buffer_scale(int32_t scale) override;
};
}
}

#endif // MIR_FRONTEND_WL_SURFACE_H
