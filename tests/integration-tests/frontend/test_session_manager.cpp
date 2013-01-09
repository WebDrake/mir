/*
 * Copyright © 2012 Canonical Ltd.
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
 * Authored by: Thomas Voss <thomas.voss@canonical.com>
 */

#include "mir/sessions/session_manager.h"
#include "mir/compositor/buffer_bundle.h"
#include "mir/surfaces/surface_controller.h"
#include "mir/surfaces/surface_stack.h"
#include "mir/surfaces/surface.h"
#include "mir/compositor/buffer_swapper.h"
#include "mir/sessions/focus_sequence.h"
#include "mir/sessions/focus_setter.h"
#include "mir/sessions/registration_order_focus_sequence.h"
#include "mir/sessions/session_container.h"
#include "mir/sessions/surface_creation_parameters.h"

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "mir_test/gmock_fixes.h"
#include "mir_test/empty_deleter.h"
#include "mir_test_doubles/mock_surface_factory.h"

namespace mc = mir::compositor;
namespace msess = mir::sessions;
namespace ms = mir::surfaces;
namespace mtd = mir::test::doubles;

namespace
{

struct MockFocusSetter: public msess::FocusSetter
{
  MOCK_METHOD1(set_focus_to, void(std::shared_ptr<msess::Session> const&));
};

}

TEST(TestSessionManagerAndFocusSelectionStrategy, cycle_focus)
{
    using namespace ::testing;
    mtd::MockSurfaceFactory organiser;
    std::shared_ptr<msess::SessionContainer> container(new msess::SessionContainer());
    msess::RegistrationOrderFocusSequence sequence(container);
    MockFocusSetter focus_changer;
    std::shared_ptr<msess::Session> new_session;

    msess::SessionManager session_manager(std::shared_ptr<msess::SurfaceFactory>(&organiser, mir::EmptyDeleter()),
                                       container,
                                       std::shared_ptr<msess::FocusSequence>(&sequence, mir::EmptyDeleter()),
                                       std::shared_ptr<msess::FocusSetter>(&focus_changer, mir::EmptyDeleter()));

    EXPECT_CALL(focus_changer, set_focus_to(_)).Times(3);

    auto session1 = session_manager.open_session("Visual Basic Studio");
    auto session2 = session_manager.open_session("Microsoft Access");
    auto session3 = session_manager.open_session("WordPerfect");

    {
      InSequence seq;
      EXPECT_CALL(focus_changer, set_focus_to(session1)).Times(1);
      EXPECT_CALL(focus_changer, set_focus_to(session2)).Times(1);
      EXPECT_CALL(focus_changer, set_focus_to(session3)).Times(1);
    }

    session_manager.focus_next();
    session_manager.focus_next();
    session_manager.focus_next();
}

TEST(TestSessionManagerAndFocusSelectionStrategy, closing_applications_transfers_focus)
{
    using namespace ::testing;
    mtd::MockSurfaceFactory organiser;
    std::shared_ptr<msess::SessionContainer> model(new msess::SessionContainer());
    msess::RegistrationOrderFocusSequence sequence(model);
    MockFocusSetter focus_changer;
    std::shared_ptr<msess::Session> new_session;

    msess::SessionManager session_manager(std::shared_ptr<msess::SurfaceFactory>(&organiser, mir::EmptyDeleter()),
                                       model,
                                       std::shared_ptr<msess::FocusSequence>(&sequence, mir::EmptyDeleter()),
                                       std::shared_ptr<msess::FocusSetter>(&focus_changer, mir::EmptyDeleter()));

    EXPECT_CALL(focus_changer, set_focus_to(_)).Times(3);

    auto session1 = session_manager.open_session("Visual Basic Studio");
    auto session2 = session_manager.open_session("Microsoft Access");
    auto session3 = session_manager.open_session("WordPerfect");

    {
      InSequence seq;
      EXPECT_CALL(focus_changer, set_focus_to(session2)).Times(1);
      EXPECT_CALL(focus_changer, set_focus_to(session1)).Times(1);
    }

    session_manager.close_session(session3);
    session_manager.close_session(session2);
}
