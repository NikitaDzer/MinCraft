#pragma once

#include "common/glfw_include.h"
#include "utils/misc.h"

#include "range/v3/algorithm/any_of.hpp"
#include "range/v3/range/concepts.hpp"
#include "range/v3/view/all.hpp"
#include "range/v3/view/filter.hpp"

#include <concepts>
#include <mutex>
#include <unordered_map>
#include <utility>

namespace glfw::input
{

enum class KeyAction : int
{
    e_press = GLFW_PRESS,
    e_release = GLFW_RELEASE,
    e_repeat = GLFW_REPEAT,
};

enum class ModifierFlagBits : int
{
    e_mod_none = 0,
    e_mod_shift = GLFW_MOD_SHIFT,
    e_mod_ctrl = GLFW_MOD_CONTROL,
    e_mod_alt = GLFW_MOD_ALT,
    e_mod_super = GLFW_MOD_SUPER,
    e_mod_caps = GLFW_MOD_CAPS_LOCK,
    e_mod_numlock = GLFW_MOD_NUM_LOCK,
};

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
};

inline ModifierFlag
operator|( const ModifierFlag& lhs, const ModifierFlag& rhs )
{
    auto copy = lhs;
    copy |= rhs;
    return copy;
}

inline ModifierFlag
operator&( const ModifierFlag& lhs, const ModifierFlag& rhs )
{
    auto copy = lhs;
    copy &= rhs;
    return copy;
}

enum class KeyState
{
    e_released,
    e_pressed,
};

using KeyIndex = int;

struct ButtonEvent
{
    ModifierFlag mods;
    KeyAction action;
};

struct TrackedKeyInfo
{
  public:
    using Events = std::vector<ButtonEvent>;

    TrackedKeyInfo() = default;

    // Kind of redundant, but whatever
    bool isPressed() const { return ( current == KeyState::e_pressed ); }
    bool isReleased() const { return ( current == KeyState::e_released ); }

    auto presses() const
    {
        return ranges::views::filter( events, []( const ButtonEvent& event ) {
            return event.action == KeyAction::e_press;
        } );
    }

    bool hasBeenPressed() const
    {
        return ranges::any_of( presses(), []( auto&& ) {
            return true;
        } ); // Hacky way to check if the range is not empty
    }

    operator bool() const { return !events.empty(); }

    void pushEvent( ButtonEvent event )
    {
        mods = event.mods;

        switch ( event.action )
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

        events.push_back( event );
    }

  public:
    KeyState current = KeyState::e_released;          // Stores whether the button is pressed at the moment
    ModifierFlag mods = ModifierFlagBits::e_mod_none; // Latest modificators used with the key
    Events events;
};

class KeyboardHandler
{
  private:
    static void keyCallbackWrapper( GLFWwindow* window, int key, int /* code */, int action, int mods )
    {
        instance( window ).keyCallback( key, static_cast<KeyAction>( action ), ModifierFlag{ mods } );
    }

    void keyCallback( KeyIndex key, KeyAction action, ModifierFlag modifier );

  public:
    using KeyInfoMap = std::unordered_map<KeyIndex, TrackedKeyInfo>;
    static KeyboardHandler& instance( GLFWwindow* window );

  private:
    static void insertKey( KeyInfoMap& map, KeyIndex key ) { map.emplace( key, TrackedKeyInfo{} ); }
    void insertKey( KeyIndex key ) { insertKey( m_tracked_keys, key ); }

  public:
    void clear()
    {
        auto g = std::lock_guard{ m_mx };
        m_tracked_keys.clear();
    }

    void monitor( KeyIndex key )
    {
        auto g = std::lock_guard{ m_mx };
        insertKey( key );
    }

    void monitor( ranges::range auto&& range )
    {
        auto g = std::lock_guard{ m_mx };
        for ( auto&& key : range )
        {
            insertKey( key );
        }
    }

  public:
    KeyInfoMap poll()
    {
        auto g = std::lock_guard{ m_mx };

        auto copy = KeyInfoMap{};
        for ( auto&& [ key, _ ] : m_tracked_keys )
        {
            insertKey( copy, key );
        }

        return std::exchange( m_tracked_keys, std::move( copy ) );
    }

  private:
    KeyInfoMap m_tracked_keys;
    mutable std::mutex m_mx; // Mutable for const-correctness

  private:
    using HandlerMap = std::unordered_map<GLFWwindow*, std::unique_ptr<KeyboardHandler>>;

    struct GlobalHandlerTable
    {
        std::once_flag initialized;
        std::mutex mutex;
        std::unique_ptr<HandlerMap> handler_map;
    };

    static GlobalHandlerTable s_handler_table;
};

}; // namespace glfw::input