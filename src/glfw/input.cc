#include "glfw/core.h"
#include "glfw/input/keyboard.h"

#include <mutex>
#include <unordered_map>

namespace glfw::input
{

// Storage for global variables
KeyboardHandler::GlobalHandlerTable KeyboardHandler::s_handler_table;

KeyboardHandler&
KeyboardHandler::instance( GLFWwindow* window )
{
    std::call_once( s_handler_table.initialized, []() {
        s_handler_table.handler_map = std::make_unique<KeyboardHandler::HandlerMap>();
    } );

    assert( s_handler_table.handler_map );

    auto g = std::unique_lock{ s_handler_table.mutex };
    auto& map = *s_handler_table.handler_map;

    if ( auto handler = map.find( window ); handler != map.end() )
    {
        assert( handler->second );
        return *handler->second;
    }

    auto handler = std::make_unique<KeyboardHandler>();
    auto& ref = *map.emplace( window, std::move( handler ) ).first->second;
    g.unlock();

    if ( !glfwSetKeyCallback( window, keyCallbackWrapper ) )
    {
        checkError();
    }

    return ref;
}

void
KeyboardHandler::keyCallback( KeyIndex key, KeyAction action, ModifierFlag modifier )
{
    if ( action == KeyAction::e_repeat )
    {
        return; // We are not really interested in repeat input, which is mainly used for text input
    }

    auto g = std::lock_guard{ m_mx };

    auto found = m_tracked_keys.find( key );
    if ( found == m_tracked_keys.end() )
    {
        return;
    }

    auto& event_info = found->second;
    event_info.pushEvent( ButtonEvent{ .mods = modifier, .action = action } );
}

} // namespace glfw::input