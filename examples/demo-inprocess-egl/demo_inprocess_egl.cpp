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
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#include "inprocess_egl_client.h"

#include "mir/run_mir.h"
#include "mir/default_server_configuration.h"

namespace me = mir::examples;

namespace
{

struct InprocessClientStarter
{
    void operator()(mir::DisplayServer *server)
    {
        // TODO: Figure out management of this why use new? racarr
        auto client = new me::InprocessEGLClient(server);
        client->start();
    }
};

struct ExampleServerConfiguration : public mir::DefaultServerConfiguration
{
    ExampleServerConfiguration(int argc, char const* argv[])
      : DefaultServerConfiguration(argc, argv)
    {
    }

    std::function<void(mir::DisplayServer *)> the_ready_to_run_handler() override
    {
        return InprocessClientStarter();
    }
};

}

int main(int argc, char const* argv[])
{
    ExampleServerConfiguration config(argc, argv);
    
    mir::run_mir(config);
}
