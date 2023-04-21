#pragma once

#include "glfw/core.h"
#include "glfw/input.h"

namespace glfw::input
{

using MouseButtonIndex = int;

struct MouseMovement
{
    double dx = 0.0;
    double dy = 0.0;
}; // MouseMovement

struct MousePosition
{
    double x = 0.0;
    double y = 0.0;

  public:
    friend MouseMovement operator-( const MousePosition& lhs, const MousePosition& rhs )
    {
        return MouseMovement{ lhs.x - rhs.x, lhs.y - rhs.y };
    }
}; // MousePosition

class MouseHandler
{
  private:
    static void buttonCallbackWrapper( GLFWwindow* window, int key, int action, int mods )
    {
        instance( window ).buttonCallback( key, static_cast<ButtonAction>( action ), ModifierFlag{ mods } );
    }

    static void positionCallbackWrapper( GLFWwindow* window, double x, double y )
    {
        instance( window ).positionCallback( x, y );
    }

  private:
    void buttonCallback( MouseButtonIndex key, ButtonAction action, ModifierFlag modifier )
    {
        assert( action != ButtonAction::k_repeat );

        {
            auto guard = std::lock_guard{ m_mx };
            m_event_map[ key ].pushEvent( ButtonEvent{ .mods = modifier, .action = action } );
        }
    } // buttonCallback

    void positionCallback( double x, double y )
    {
        auto guard = std::lock_guard{ m_mx };
        m_position = MousePosition{ .x = x, .y = y };
    } // positionCallback

  private:
    MouseHandler( GLFWwindow* window )
        : m_window{ window }
    {
        glfwSetMouseButtonCallback( window, buttonCallbackWrapper );
        glfwSetCursorPosCallback( window, positionCallbackWrapper );
    }

    MousePosition updatePosition() { return std::exchange( m_old_position, m_position ); }

  public:
    using ButtonEventMap = std::unordered_map<MouseButtonIndex, ButtonEventInfo>;
    static MouseHandler& instance( GLFWwindow* window )
    {
        return *s_handler_table.lookup( window, [ window ]() {
            return std::unique_ptr<MouseHandler>{ new MouseHandler{ window } };
        } );
    } // instance

  public:
    void setHidden() { glfwSetInputMode( m_window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN ); }
    void setNormal() { glfwSetInputMode( m_window, GLFW_CURSOR, GLFW_CURSOR_NORMAL ); }

  public:
    struct PollResult
    {
        ButtonEventMap buttons;
        MousePosition position;
        MouseMovement movement;
    }; // PollResult

    PollResult poll()
    {
        ButtonEventMap old_map;

        {
            auto guard = std::lock_guard{ m_mx };
            old_map = std::exchange( m_event_map, ButtonEventMap{} );
        }

        auto old_position = updatePosition();
        auto movement = m_position - old_position;

        return PollResult{ std::move( old_map ), m_position, movement };
    } // poll

  private:
    ButtonEventMap m_event_map;
    MousePosition m_position;     // Current position
    MousePosition m_old_position; // Position from the previous poll call
    GLFWwindow* const m_window;
    std::mutex m_mx;

  private:
    static inline detail::GlobalHandlerTable<std::unique_ptr<MouseHandler>> s_handler_table;
}; // MouseHandler

} // namespace glfw::input