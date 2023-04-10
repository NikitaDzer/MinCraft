#include <spdlog/cfg/env.h>
#include <spdlog/fmt/bundled/format.h>

#include "common/glfw_include.h"
#include "input/keyboard.h"
#include "window/window.h"

#include <array>
#include <atomic>
#include <chrono>
#include <iostream>
#include <string_view>
#include <thread>

using namespace std::literals::chrono_literals;

int
main()
try
{
    spdlog::cfg::load_env_levels();
    wnd::glfw::WindowManager::initialize();

    fmt::print( "Required extensions:\n" );

    // clang-format off
    for ( auto extensions = wnd::glfw::WindowManager::getRequiredExtensions(); 
          auto&& extension : extensions )
    {
        fmt::print( "{}\n", extension );
    }
    // clang-format on

    wnd::glfw::Window window{ { .title = "Mincraft V2" } };

    std::jthread printer{ [ & ]( std::stop_token token ) {
        using input::glfw::KeyState;

        auto& keyboard = input::glfw::KeyboardHandler::instance( window );
        keyboard.monitor( std::to_array<input::glfw::KeyMonitorInfo>(
            { { GLFW_KEY_A, KeyState::e_clicked }, { GLFW_KEY_D, KeyState::e_held_down } } ) );

        auto state_to_string = []( KeyState st ) {
            using input::glfw::KeyState;
            switch ( st )
            {
            case KeyState::e_clicked:
                return "Clicked";
            case KeyState::e_held_down:
                return "Held Down";
            case KeyState::e_idle:
                return "Idle";
            }
        };

        while ( !token.stop_requested() )
        {
            for ( auto&& [ key, state ] : keyboard.poll() )
            {
                std::string_view key_name = glfwGetKeyName( key, 0 );
                std::cout << fmt::format( "Key: {}, Action: {}\n", key_name, state_to_string( state ) ) << std::flush;
            }

            std::this_thread::sleep_for( 25ms );
        }
    } };

    while ( window.running() )
    {
        glfwPollEvents();
    }

    printer.request_stop();

} catch ( wnd::Error& e )
{
    fmt::print( "GLFW window system error: {}\n", e.what() );
} catch ( std::exception& e )
{
    fmt::print( "Error: {}\n", e.what() );
}
