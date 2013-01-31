/*
 * Copyright © 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#include "mir/graphics/display.h"
#include "mir/graphics/display_buffer.h"
#include "src/graphics/gbm/gbm_platform.h"

#include "mir_test/egl_mock.h"
#include "mir_test/gl_mock.h"
#include "mir_test_doubles/null_display_listener.h"

#include "mock_drm.h"
#include "mock_gbm.h"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

namespace mg = mir::graphics;
namespace mgg = mir::graphics::gbm;
namespace geom = mir::geometry;
namespace mtd = mir::test::doubles;

namespace
{

class GBMDisplayMultiMonitorTest : public ::testing::Test
{
public:
    GBMDisplayMultiMonitorTest()
        : null_listener{std::make_shared<mtd::NullDisplayListener>()}
    {
        using namespace testing;

        /* Needed for display start-up */
        ON_CALL(mock_egl, eglChooseConfig(_,_,_,1,_))
            .WillByDefault(DoAll(SetArgPointee<2>(mock_egl.fake_configs[0]),
                                 SetArgPointee<4>(1),
                                 Return(EGL_TRUE)));

        const char* egl_exts = "EGL_KHR_image EGL_KHR_image_base EGL_MESA_drm_image";
        const char* gl_exts = "GL_OES_texture_npot GL_OES_EGL_image";

        ON_CALL(mock_egl, eglQueryString(_,EGL_EXTENSIONS))
            .WillByDefault(Return(egl_exts));
        ON_CALL(mock_gl, glGetString(GL_EXTENSIONS))
            .WillByDefault(Return(reinterpret_cast<const GLubyte*>(gl_exts)));

        /*
         * Silence uninteresting calls called when cleaning up resources in
         * the MockGBM destructor, and which are not handled by NiceMock<>.
         */
        EXPECT_CALL(mock_gbm, gbm_bo_get_device(_))
            .Times(AtLeast(0));
        EXPECT_CALL(mock_gbm, gbm_device_get_fd(_))
            .Times(AtLeast(0));
    }

    void setup_outputs(int n)
    {
        mgg::FakeDRMResources& resources(mock_drm.fake_drm);

        modes0.clear();
        modes0.push_back(mgg::FakeDRMResources::create_mode(1920, 1080, 138500, 2080, 1111));
        modes0.push_back(mgg::FakeDRMResources::create_mode(1920, 1080, 148500, 2200, 1125));
        modes0.push_back(mgg::FakeDRMResources::create_mode(1680, 1050, 119000, 1840, 1080));
        modes0.push_back(mgg::FakeDRMResources::create_mode(832, 624, 57284, 1152, 667));

        geom::Size const connector_physical_size_mm{geom::Width{1597}, geom::Height{987}};

        resources.reset();

        uint32_t const crtc_base_id{10};
        uint32_t const encoder_base_id{20};
        uint32_t const connector_base_id{30};

        for (int i = 0; i < n; i++)
        {
            uint32_t const crtc_id{crtc_base_id + i};
            uint32_t const encoder_id{encoder_base_id + i};
            uint32_t const connector_id{connector_base_id + i};

            crtc_ids.push_back(crtc_id);
            resources.add_crtc(crtc_id, drmModeModeInfo());

            encoder_ids.push_back(encoder_id);
            resources.add_encoder(encoder_id, crtc_id);

            connector_ids.push_back(connector_id);
            resources.add_connector(connector_id, DRM_MODE_CONNECTED, encoder_id,
                                    modes0, connector_physical_size_mm);
        }

        resources.prepare();
    }


    testing::NiceMock<mir::EglMock> mock_egl;
    testing::NiceMock<mir::GLMock> mock_gl;
    testing::NiceMock<mgg::MockDRM> mock_drm;
    testing::NiceMock<mgg::MockGBM> mock_gbm;
    std::shared_ptr<mg::DisplayListener> const null_listener;

    std::vector<drmModeModeInfo> modes0;
    std::vector<drmModeModeInfo> modes_empty;
    std::vector<uint32_t> crtc_ids;
    std::vector<uint32_t> encoder_ids;
    std::vector<uint32_t> connector_ids;
};

}

TEST_F(GBMDisplayMultiMonitorTest, create_display_sets_all_connected_crtcs)
{
    using namespace testing;

    int const num_outputs{3};
    uint32_t const fb_id{66};

    setup_outputs(num_outputs);

    /* Create DRM FBs */
    EXPECT_CALL(mock_drm, drmModeAddFB(mock_drm.fake_drm.fd(),
                                       _, _, _, _, _, _, _))
        .WillRepeatedly(DoAll(SetArgPointee<7>(fb_id), Return(0)));

    ExpectationSet crtc_setups;

    /* All crtcs are set */
    for (int i = 0; i < num_outputs; i++)
    {
        crtc_setups += EXPECT_CALL(mock_drm,
                                   drmModeSetCrtc(mock_drm.fake_drm.fd(),
                                                  crtc_ids[i], fb_id,
                                                  _, _,
                                                  Pointee(connector_ids[i]),
                                                  _, _))
                           .Times(AtLeast(1));
    }

    /* All crtcs are restored at teardown */
    for (int i = 0; i < num_outputs; i++)
    {
        EXPECT_CALL(mock_drm, drmModeSetCrtc(mock_drm.fake_drm.fd(),
                                             crtc_ids[i], Ne(fb_id),
                                             _, _,
                                             Pointee(connector_ids[i]),
                                             _, _))
            .Times(1)
            .After(crtc_setups);
    }

    auto platform = std::make_shared<mgg::GBMPlatform>(null_listener);
    auto display = platform->create_display();
}

TEST_F(GBMDisplayMultiMonitorTest, post_update_flips_all_connected_crtcs)
{
    using namespace testing;

    int const num_outputs{3};
    uint32_t const fb_id{66};

    setup_outputs(num_outputs);

    /* Create DRM FBs */
    EXPECT_CALL(mock_drm, drmModeAddFB(mock_drm.fake_drm.fd(),
                                       _, _, _, _, _, _, _))
        .WillRepeatedly(DoAll(SetArgPointee<7>(fb_id), Return(0)));

    /* All crtcs are flipped */
    for (int i = 0; i < num_outputs; i++)
    {
        EXPECT_CALL(mock_drm, drmModePageFlip(mock_drm.fake_drm.fd(),
                                              crtc_ids[i], fb_id,
                                              _, _))
            .Times(1)
            .WillOnce(Return(0));
    }

    auto platform = std::make_shared<mgg::GBMPlatform>(null_listener);
    auto display = platform->create_display();

    display->for_each_display_buffer([](mg::DisplayBuffer& buffer)
    {
        buffer.post_update();
    });
}
