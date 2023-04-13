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
        using input::glfw::KeyAction;

        auto& keyboard = input::glfw::KeyboardHandler::instance( window );
        keyboard.monitor( std::to_array<input::glfw::KeyIndex>( { GLFW_KEY_A, GLFW_KEY_D } ) );

        auto state_to_string = []( KeyState st ) {
            switch ( st )
            {
            case KeyState::e_pressed:
                return "Pressed";
            case KeyState::e_released:
                return "Released";
            }
        };

        auto action_to_string = []( KeyAction st ) {
            switch ( st )
            {
            case KeyAction::e_press:
                return "Press";
            case KeyAction::e_release:
                return "Release";
            case KeyAction::e_repeat:
                return "Repeat";
            }
        };

        while ( !token.stop_requested() )
        {
            for ( auto&& [ key, info ] : keyboard.poll() )
            {
                std::string_view key_name = glfwGetKeyName( key, 0 );
                if ( info.hasBeenPressed() )
                {
                    std::cout << fmt::format( "Key: {}, State: {}\n", key_name, state_to_string( info.current ) );
                    for ( auto i = 0; auto&& press : info.presses() )
                    {
                        std::cout << fmt::format( "Event [{}], State: {}\n", i++, action_to_string( press.action ) );
                    }
                }
            }

            std::this_thread::sleep_for( 25ms );
        }
    } };

    while ( window.running() )
    {
        glfwWaitEvents();
    }

    printer.request_stop();

} catch ( wnd::Error& e )
{
    fmt::print( "GLFW window system error: {}\n", e.what() );
} catch ( std::exception& e )
{
    fmt::print( "Error: {}\n", e.what() );
}
