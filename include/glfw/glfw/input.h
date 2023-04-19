#pragma once

#include "common/glfw_include.h"
#include "utils/misc.h"

#include <range/v3/algorithm/any_of.hpp>
#include <range/v3/view/filter.hpp>

#include <string_view>
#include <utility>

namespace glfw::input
{

enum class ButtonAction : int
{
    e_press = GLFW_PRESS,
    e_release = GLFW_RELEASE,
    e_repeat = GLFW_REPEAT,
}; // ButtonAction

enum class ButtonState
{
    e_released,
    e_pressed,
}; // ButtonState

constexpr std::string_view
buttonStateToString( ButtonState st )
{
    switch ( st )
    {
    case ButtonState::e_pressed:
        return "Pressed";
    case ButtonState::e_released:
        return "Released";
    default:
        assert( 0 && "Unhandled enum case" );
        std::terminate();
        break;
    }
}; // buttonStateToString

constexpr std::string_view
buttonActionToString( ButtonAction st )
{
    switch ( st )
    {
    case ButtonAction::e_press:
        return "Press";
    case ButtonAction::e_release:
        return "Release";
    case ButtonAction::e_repeat:
        return "Repeat";
    default:
        assert( 0 && "Unhandled enum case" );
        std::terminate();
        break;
    }
}; // buttonActionToString

enum class ModifierFlagBits : int
{
    e_mod_none = 0,
    e_mod_shift = GLFW_MOD_SHIFT,
    e_mod_ctrl = GLFW_MOD_CONTROL,
    e_mod_alt = GLFW_MOD_ALT,
    e_mod_super = GLFW_MOD_SUPER,
    e_mod_caps = GLFW_MOD_CAPS_LOCK,
    e_mod_numlock = GLFW_MOD_NUM_LOCK,
}; // ModifierFlagBits

class ModifierFlag
{
  public:
    using Underlying = std::underlying_type_t<ModifierFlagBits>;

  public:
    ModifierFlag( ModifierFlagBits flag = ModifierFlagBits::e_mod_none )
        : m_underlying{ utils::toUnderlying( flag ) }
    {
    }

    explicit ModifierFlag( Underlying underlying )
        : m_underlying{ underlying }
    {
    }

  public:
    bool operator==( const ModifierFlag& rhs ) const = default;

    ModifierFlag& operator|=( const ModifierFlag& rhs ) &
    {
        m_underlying |= rhs.m_underlying;
        return *this;
    }

    ModifierFlag& operator&=( const ModifierFlag& rhs ) &
    {
        m_underlying &= rhs.m_underlying;
        return *this;
    }

    bool isSet( const ModifierFlagBits& bit ) const { return m_underlying & utils::toUnderlying( bit ); }

  private:
    Underlying m_underlying;
}; // ModifierFlag

inline ModifierFlag
operator|( const ModifierFlag& lhs, const ModifierFlag& rhs )
{
    auto copy = lhs;
    copy |= rhs;
    return copy;
} // operator|

inline ModifierFlag
operator&( const ModifierFlag& lhs, const ModifierFlag& rhs )
{
    auto copy = lhs;
    copy &= rhs;
    return copy;
} // operator&

struct ButtonEvent
{
    ModifierFlag mods;
    ButtonAction action;
}; // ButtonEvent

struct ButtonEventInfo
{
  public:
    using Events = std::vector<ButtonEvent>;

    ButtonEventInfo() = default;

    // Kind of redundant, but whatever
    bool isPressed() const { return ( current == ButtonState::e_pressed ); }   // isPressed
    bool isReleased() const { return ( current == ButtonState::e_released ); } // isReleased

    auto presses() const
    {
        return ranges::views::filter( events, []( const ButtonEvent& event ) {
            return event.action == ButtonAction::e_press;
        } );
    } // presses

    bool hasBeenPressed() const
    {
        return ranges::any_of( presses(), []( auto&& ) {
            return true;
        } ); // Hacky way to check if the range is not empty
    }        // hasBeenPressed

    operator bool() const { return !events.empty(); }

    void pushEvent( ButtonEvent event )
    {
        mods = event.mods;

        switch ( event.action )
        {
        case ButtonAction::e_press:
            current = ButtonState::e_pressed;
            break;
        case ButtonAction::e_release:
            current = ButtonState::e_released;
            break;
        default:
            break;
        }

        events.push_back( event );
    } // pushEvent

  public:
    ButtonState current = ButtonState::e_released;    // Stores whether the button is pressed at the moment
    ModifierFlag mods = ModifierFlagBits::e_mod_none; // Latest modificators used with the key
    Events events;
}; // ButtonEventInfo

}; // namespace glfw::input