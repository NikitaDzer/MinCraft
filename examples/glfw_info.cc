#include <spdlog/cfg/env.h>

#include "common/glfw_include.h"
#include "window/window.h"

int
main()
try
{
    spdlog::cfg::load_env_levels();
    wnd::WindowManager::initialize();

    fmt::print( "Required extensions:\n" );

    // clang-format off
    for ( auto extensions = wnd::WindowManager::getRequiredExtensions(); 
          auto&& extension : extensions )
    {
        fmt::print( "{}\n", extension );
    }
    // clang-format on

    wnd::Window window{ { .title = "Mincraft V2" } };

    while ( window.running() )
    {
        glfwPollEvents();
    }

    return 0;
} catch ( wnd::Error& e )
{
    fmt::print( "GLFW window system error: {}\n", e.what() );
} catch ( std::exception& e )
{
    fmt::print( "Error: {}\n", e.what() );
}
