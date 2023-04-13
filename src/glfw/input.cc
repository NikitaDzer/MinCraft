#include "input/keyboard.h"

#include <mutex>
#include <unordered_map>

namespace input::glfw
{

// Storage for global variables
KeyboardHandler::HandlerMap KeyboardHandler::s_keyboard_handler_map;
std::mutex KeyboardHandler::s_handler_map_mx;

KeyboardHandler&
KeyboardHandler::instance( GLFWwindow* window )
{
    auto g = std::unique_lock{ s_handler_map_mx };

    if ( auto handler = s_keyboard_handler_map.find( window ); handler != s_keyboard_handler_map.end() )
    {
        auto* ptr = handler->second.get();
        assert( ptr && "Class invariant broken" );
        return *ptr;
    }

    auto& ref = *s_keyboard_handler_map.emplace( window, std::make_unique<KeyboardHandler>() ).first->second;
    g.unlock();

    glfwSetKeyCallback( window, keyCallbackWrapper );
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

    auto& [ current, mods, events ] = found->second;

    switch ( action )
    {
    case KeyAction::e_press:
        current = KeyState::e_pressed;
        break;
    case KeyAction::e_release:
        current = KeyState::e_released;
        break;
    default:
        break;
    }

    mods = modifier;
    auto event = ButtonEvent{ .mods = modifier, .action = action };
    events.push_back( event );
}

} // namespace input::glfw