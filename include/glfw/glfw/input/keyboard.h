#pragma once

#include "glfw/core.h"
#include "glfw/input.h"

#include <range/v3/algorithm/copy.hpp>
#include <range/v3/range/concepts.hpp>

#include <concepts>
#include <mutex>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace glfw::input
{

using KeyIndex = int;

class KeyboardHandler
{
  private:
    static void keyCallbackWrapper( GLFWwindow* window, int key, int /* code */, int action, int mods )
    {
        instance( window ).keyCallback( key, static_cast<ButtonAction>( action ), ModifierFlag{ mods } );
    } // keyCallbackWrapper

    void keyCallback( KeyIndex key, ButtonAction action, ModifierFlag modifier )
    {
        if ( action == ButtonAction::k_repeat )
        {
            return; // We are not really interested in repeat input, which is mainly used for text input
        }

        {
            auto guard = std::lock_guard{ m_mx };
            m_button_events[ key ].pushEvent( ButtonEvent{ .mods = modifier, .action = action } );
        }
    } // keyCallback

    using TrackedKeysSet = std::unordered_set<KeyIndex>;
    using KeyEventMap = std::unordered_map<KeyIndex, ButtonEventInfo>;

  private:
    KeyboardHandler( GLFWwindow* window ) { glfwSetKeyCallback( window, keyCallbackWrapper ); }

  public:
    static KeyboardHandler& instance( GLFWwindow* window )
    {
        return *s_handler_table.lookup( window, [ window ]() {
            return std::unique_ptr<KeyboardHandler>{ new KeyboardHandler{ window } };
        } );
    } // instance

  public:
    // Note that these functions don't have to be synchronized, because only one thread should call clear and/or
    // monitor.

    void clear()
    {
        m_tracked_keys.clear(); // Remove all tracked keys.
    }

    void monitor( KeyIndex key )
    {
        m_tracked_keys.insert( key ); // Notify when this key has any events associated with it.
    }

    void monitor( ranges::range auto&& range )
    {
        ranges::copy( range, std::inserter( m_tracked_keys, m_tracked_keys.end() ) );
    }

  public:
    KeyEventMap poll()
    {
        KeyEventMap old_map;

        {
            auto guard = std::lock_guard{ m_mx };
            old_map = std::exchange( m_button_events, KeyEventMap{} );
        }

        std::erase_if( old_map, [ this ]( const auto& item ) {
            auto&& key = item.first;
            return !m_tracked_keys.contains( key );
        } );

        return old_map;
    } // poll

  private:
    TrackedKeysSet m_tracked_keys;
    KeyEventMap m_button_events;
    std::mutex m_mx;

  private:
    static inline detail::GlobalHandlerTable<std::unique_ptr<KeyboardHandler>> s_handler_table;
};

class KeyboardStateTracker
{
  public:
    KeyboardStateTracker( GLFWwindow* window )
        : m_handler_ptr{ &glfw::input::KeyboardHandler::instance( window ) }
    {
        assert( window );
    }

    KeyboardStateTracker( glfw::input::KeyboardHandler& handler )
        : m_handler_ptr{ &handler }
    {
    }

  public:
    template <ranges::range Range>
        requires std::same_as<ranges::range_value_t<Range>, glfw::input::KeyIndex>
    void monitor( Range&& keys )
    {
        m_state_map.clear();
        for ( auto&& key : keys )
        {
            m_state_map.insert( std::pair{ key, glfw::input::ButtonState::k_released } );
        }

        m_handler_ptr->monitor( keys );
    }

    auto loggingPoll() const
    {
        std::stringstream ss;

        auto print = [ &ss ]( std::string_view key, auto info ) {
            ss << fmt::format( "Key: {}, State: {}\n", key, glfw::input::buttonStateToString( info.current ) );
            if ( info.hasBeenPressed() )
            {
                for ( auto i = 0; auto&& press : info.presses() )
                {
                    auto action_string = glfw::input::buttonActionToString( press.action );
                    ss << fmt::format( "Event [{}], State: {}\n", i++, action_string );
                }
            }
        };

        auto poll_result = m_handler_ptr->poll();

        for ( auto&& [ key, info ] : poll_result )
        {
            auto* name_cstr = glfwGetKeyName( key, 0 /* Ignore scancode */ );

            if ( !name_cstr )
            {
                continue;
            }

            print( name_cstr, info );
        }

        if ( auto msg = ss.str(); !msg.empty() )
        {
            spdlog::debug( msg );
        }

        return poll_result;
    }

    void update()
    {
        for ( auto&& [ key, info ] : loggingPoll() )
        {
            m_state_map[ key ] = info.current;
        }
    }

    glfw::input::ButtonState getState( glfw::input::KeyIndex key ) const
    {
        if ( auto found = m_state_map.find( key ); found != m_state_map.end() )
        {
            return found->second;
        }

        throw std::out_of_range{ "KeyboardStateTracker::getState: key not found" };
    }

    bool isPressed( glfw::input::KeyIndex key ) const { return getState( key ) == glfw::input::ButtonState::k_pressed; }

  private:
    glfw::input::KeyboardHandler* m_handler_ptr;
    std::unordered_map<glfw::input::KeyIndex, glfw::input::ButtonState> m_state_map;
};

}; // namespace glfw::input