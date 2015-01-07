/*
 * Copyright © 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include "device_quirks.h"
#include "output_builder.h"
#include "display_resource_factory.h"
#include "display_buffer.h"
#include "display_device.h"
#include "framebuffers.h"
#include "real_hwc_wrapper.h"
#include "hwc_report.h"
#include "hwc_layers.h"
#include "hwc_configuration.h"

#include "mir/graphics/display_buffer.h"
#include "mir/graphics/egl_resources.h"

namespace mg = mir::graphics;
namespace mga = mir::graphics::android;
namespace geom = mir::geometry;

mga::OutputBuilder::OutputBuilder(
    std::shared_ptr<mga::GraphicBufferAllocator> const& buffer_allocator,
    std::shared_ptr<mga::DisplayResourceFactory> const& res_factory,
    mga::OverlayOptimization overlay_optimization,
    std::shared_ptr<HwcReport> const& hwc_report)
    : buffer_allocator(buffer_allocator),
      res_factory(res_factory),
      hwc_report(hwc_report),
      force_backup_display(false),
      overlay_optimization(overlay_optimization)
{
    try
    {
        hwc_native = res_factory->create_hwc_native_device();
        hwc_wrapper = std::make_shared<mga::RealHwcWrapper>(hwc_native, hwc_report);
    } catch (...)
    {
        force_backup_display = true;
    }

    if (force_backup_display || hwc_native->common.version == HWC_DEVICE_API_VERSION_1_0)
    {
        fb_native = res_factory->create_fb_native_device();
        framebuffers = std::make_shared<mga::Framebuffers>(buffer_allocator, fb_native);
    }
    else
    {
        mga::PropertiesOps ops;
        mga::DeviceQuirks quirks(ops);
        mga::HwcBlankingControl hwc_config{hwc_wrapper};
        auto attribs = hwc_config.active_attribs_for(mga::DisplayName::primary);
        framebuffers = std::make_shared<mga::Framebuffers>(
            buffer_allocator, attribs.pixel_size, attribs.vrefresh_hz, quirks.num_framebuffers());
    }
}

MirPixelFormat mga::OutputBuilder::display_format()
{
    return framebuffers->fb_format();
}

std::unique_ptr<mga::ConfigurableDisplayBuffer> mga::OutputBuilder::create_display_buffer(
    GLProgramFactory const& gl_program_factory,
    GLContext const& gl_context)
{
    std::shared_ptr<mga::DisplayDevice> device;
    if (force_backup_display)
    {
        device = res_factory->create_fb_device(fb_native);
        hwc_report->report_legacy_fb_module();
    }
    else
    {
        switch (hwc_native->common.version)
        {
            case HWC_DEVICE_API_VERSION_1_0:
                device = res_factory->create_hwc_fb_device(hwc_wrapper, fb_native);
            break;

            case HWC_DEVICE_API_VERSION_1_1:
            case HWC_DEVICE_API_VERSION_1_2:
                device = res_factory->create_hwc_device(
                    hwc_wrapper, std::make_shared<mga::IntegerSourceCrop>());
            break;

            default:
            case HWC_DEVICE_API_VERSION_1_3:
                device = res_factory->create_hwc_device(
                    hwc_wrapper, std::make_shared<mga::FloatSourceCrop>());
            break;
        }

        hwc_report->report_hwc_version(hwc_native->common.version);
    }

    auto native_window = res_factory->create_native_window(framebuffers);
    return std::unique_ptr<mga::DisplayBuffer>(new DisplayBuffer(
        framebuffers,
        device,
        native_window,
        gl_context,
        gl_program_factory,
        overlay_optimization));
}