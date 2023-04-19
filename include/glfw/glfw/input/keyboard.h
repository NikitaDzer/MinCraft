#pragma once

#include "common/glfw_include.h"
#include "glfw/core.h"

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
    }

    void keyCallback( KeyIndex key, ButtonAction action, ModifierFlag modifier )
    {
        if ( action == ButtonAction::e_repeat )
        {
            return; // We are not really interested in repeat input, which is mainly used for text input
        }

        {
            auto g = std::lock_guard{ m_mx };
            m_button_events[ key ].pushEvent( ButtonEvent{ .mods = modifier, .action = action } );
        }
    }

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
    }

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
            auto g = std::lock_guard{ m_mx };
            old_map = std::exchange( m_button_events, KeyEventMap{} );
        }

        std::erase_if( old_map, [ this ]( const auto& item ) {
            auto&& key = item.first;
            return !m_tracked_keys.contains( key );
        } );

        return old_map;
    }

  private:
    TrackedKeysSet m_tracked_keys;
    KeyEventMap m_button_events;
    mutable std::mutex m_mx;

  private:
    static inline detail::GlobalHandlerTable<std::unique_ptr<KeyboardHandler>> s_handler_table;
};

}; // namespace glfw::input