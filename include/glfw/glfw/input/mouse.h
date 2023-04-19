#pragma once

#include "glfw/core.h"

namespace glfw::input
{

using MouseButtonIndex = int;

struct MouseMovement
{
    double dx = 0.0;
    double dy = 0.0;
};

struct MousePosition
{
    double x = 0.0;
    double y = 0.0;

  public:
    friend MouseMovement operator-( const MousePosition& lhs, const MousePosition& rhs )
    {
        return MouseMovement{ lhs.x - rhs.x, lhs.y - rhs.y };
    }
};

class MouseHandler
{
  private:
    static void buttonCallbackWrapper( GLFWwindow* window, int key, int action, int mods )
    {
        instance( window ).buttonCallback( key, static_cast<ButtonAction>( action ), ModifierFlag{ mods } );
    }

    void buttonCallback( MouseButtonIndex key, ButtonAction action, ModifierFlag modifier )
    {
        assert( action != ButtonAction::e_repeat );

        {
            auto g = std::lock_guard{ m_mx };
            m_event_map[ key ].pushEvent( ButtonEvent{ .mods = modifier, .action = action } );
        }
    }

  private:
    MouseHandler( GLFWwindow* window )
        : m_window{ window }
    {
        glfwSetMouseButtonCallback( window, buttonCallbackWrapper );
        m_position = getPosition();
    }

    MousePosition updatePosition() { return std::exchange( m_position, getPosition() ); }

  public:
    using ButtonEventMap = std::unordered_map<MouseButtonIndex, ButtonEventInfo>;
    static MouseHandler& instance( GLFWwindow* window )
    {
        return *s_handler_table.lookup( window, [ window ]() {
            return std::unique_ptr<MouseHandler>{ new MouseHandler{ window } };
        } );
    }

  public:
    void setHidden() { glfwSetInputMode( m_window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN ); }
    void setNormal() { glfwSetInputMode( m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL ); }

  private:
    MousePosition getPosition()
    {
        MousePosition ret;
        glfwGetCursorPos( m_window, &ret.x, &ret.y );
        return ret;
    }

  public:
    struct PollResult
    {
        ButtonEventMap buttons;
        MousePosition position;
        MouseMovement movement;
    };

    PollResult poll()
    {
        ButtonEventMap old_map;

        {
            auto g = std::lock_guard{ m_mx };
            old_map = std::exchange( m_event_map, ButtonEventMap{} );
        }

        auto old_position = updatePosition();
        auto movement = m_position - old_position;

        return PollResult{ std::move( old_map ), m_position, movement };
    }

  private:
    ButtonEventMap m_event_map;
    MousePosition m_position;
    GLFWwindow* const m_window;
    mutable std::mutex m_mx;

  private:
    static inline detail::GlobalHandlerTable<std::unique_ptr<MouseHandler>> s_handler_table;
};

} // namespace glfw::input