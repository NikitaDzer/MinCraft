#include <spdlog/cfg/env.h>
#include <spdlog/fmt/bundled/format.h>

#include "common/glfw_include.h"
#include "glfw/input/keyboard.h"
#include "glfw/input/mouse.h"
#include "glfw/window.h"

#include <array>
#include <atomic>
#include <chrono>
#include <iostream>
#include <string_view>
#include <thread>

using namespace std::literals::chrono_literals;

namespace
{

using glfw::ButtonAction;
using glfw::ButtonState;

auto state_to_string = []( ButtonState st ) {
    switch ( st )
    {
    case ButtonState::e_pressed:
        return "Pressed";
    case ButtonState::e_released:
        return "Released";
    }
};

auto action_to_string = []( ButtonAction st ) {
    switch ( st )
    {
    case ButtonAction::e_press:
        return "Press";
    case ButtonAction::e_release:
        return "Release";
    case ButtonAction::e_repeat:
        return "Repeat";
    }
};

auto
launch_thread( glfw::wnd::Window& window )
{
    auto loop_work = [ &window ]( std::stop_token token ) {
        auto& keyboard = glfw::input::KeyboardHandler::instance( window );
        keyboard.monitor( std::to_array( { GLFW_KEY_A, GLFW_KEY_D } ) );
        auto& mouse = glfw::input::MouseHandler::instance( window );
        mouse.setNormal();

        while ( !token.stop_requested() )
        {
            auto mouse_poll = mouse.poll();

            auto print = []( std::string key, auto info ) {
                if ( info.hasBeenPressed() )
                {
                    std::cout << fmt::format( "Key: {}, State: {}\n", key, state_to_string( info.current ) );
                    for ( auto i = 0; auto&& press : info.presses() )
                    {
                        std::cout << fmt::format( "Event [{}], State: {}\n", i++, action_to_string( press.action ) );
                    }
                }
            };

            for ( auto&& [ key, info ] : keyboard.poll() )
            {
                std::string_view key_name = glfwGetKeyName( key, 0 );
                print( std::string{ key_name }, info );
            }

            for ( auto&& [ key, info ] : mouse_poll.buttons )
            {
                print( fmt::format( "Mouse button [{}]", key ), info );
            }

            do
            {
                auto [ x, y ] = mouse_poll.position;
                auto [ dx, dy ] = mouse_poll.movement;

                if ( dx == 0.0 && dy == 0.0 )
                {
                    break;
                }

                std::cout << fmt::format( "Mouse position: [x = {}, y = {}] -- ", x, y );
                std::cout << fmt::format( "Mouse movement: [dx = {}, dy = {}]\n", dx, dy );
            } while ( false );

            std::this_thread::sleep_for( 25ms );
        }
    };

    return std::jthread{ loop_work };
}

} // namespace

int
main()
try
{
    spdlog::cfg::load_env_levels();
    auto glfw_instance = glfw::Instance{};

    fmt::print( "Required extensions:\n" );

    // clang-format off
    for ( auto extensions = glfw_instance.getRequiredExtensions(); 
          auto&& extension : extensions )
    {
        fmt::print( "{}\n", extension );
    }
    // clang-format on

    auto window = glfw::wnd::Window{ { .title = "Mincraft V2" } };
    auto printer = launch_thread( window );

    while ( window.running() )
    {
        glfwWaitEvents();
    }

    printer.request_stop();

} catch ( glfw::Error& e )
{
    fmt::print( "GLFW window system error: {}\n", e.what() );
} catch ( std::exception& e )
{
    fmt::print( "Error: {}\n", e.what() );
}
