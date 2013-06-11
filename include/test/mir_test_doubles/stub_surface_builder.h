/*
 * Copyright © 2013 Canonical Ltd.
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
 * Authored by: Alan Griffiths <alan@octopull.co.uk>
 */


#ifndef MIR_TEST_DOUBLES_STUB_SURFACE_BUILDER_H_
#define MIR_TEST_DOUBLES_STUB_SURFACE_BUILDER_H_

#include "mir/shell/surface_builder.h"
#include "mir/surfaces/surface.h"
#include "mir/shell/surface_creation_parameters.h"

#include "mir_test_doubles/null_buffer_bundle.h"

namespace mir
{
namespace test
{
namespace doubles
{

class StubSurfaceBuilder : public shell::SurfaceBuilder
{
public:
    StubSurfaceBuilder() :
        buffer_bundle(std::make_shared<NullBufferStream>()),
        dummy_surface()
    {
    }

    std::weak_ptr<surfaces::Surface> create_surface(shell::SurfaceCreationParameters const& param)
    {
        dummy_surface = std::make_shared<surfaces::Surface>(param.name, param.top_left, buffer_bundle, 
            std::shared_ptr<input::InputChannel>(), []{});
        return dummy_surface;
    }

    void destroy_surface(std::weak_ptr<surfaces::Surface> const& )
    {
    }
private:
    std::shared_ptr<surfaces::BufferStream> const buffer_bundle;
    std::shared_ptr<surfaces::Surface>  dummy_surface;
};
}
}
}


#endif /* MIR_TEST_DOUBLES_STUB_SURFACE_BUILDER_H_ */
