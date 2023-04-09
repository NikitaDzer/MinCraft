#pragma once

#include "common/glfw_include.h"

#include "range/v3/range/concepts.hpp"
#include "range/v3/view/all.hpp"
#include "range/v3/view/filter.hpp"

#include <concepts>
#include <mutex>
#include <unordered_map>

namespace input::glfw
{

class KeyboardHandler
{
  public:
    enum class KeyState
    {
        e_idle,
        e_held_down,
        e_clicked
    };

    using KeyIndex = int;

  private:
    struct TrackedKeyInfo
    {
        KeyState current;
        const KeyState lookfor;

      public:
        TrackedKeyInfo( KeyState current_param, KeyState lookfor_param )
            : current{ current_param },
              lookfor{ lookfor_param }
        {
        }
    };

  private:
    std::unordered_map<KeyIndex, TrackedKeyInfo> m_tracked_keys;
    std::mutex m_mx;

  private:
    KeyboardHandler() = default;

  public:
    static KeyboardHandler& instance()
    {
        static KeyboardHandler handler;
        return handler;
    }

    void bind( GLFWwindow* window )
    {
        auto key_callback = []( auto*, int key, int /* code */, int action, int /* mods */ ) {
            auto& self = instance();
            auto g = std::lock_guard{ self.m_mx };
            auto found = self.m_tracked_keys.find( key );
            if ( found == self.m_tracked_keys.end() )
            {
                return;
            }

            TrackedKeyInfo& key_info = found->second;
            if ( action == GLFW_PRESS )
            {
                key_info.current = KeyState::e_clicked;
            } else if ( action == GLFW_RELEASE )
            {
                key_info.current = KeyState::e_clicked;
            }
        };

        glfwSetKeyCallback( window, key_callback );
    }

  public:
    void clear()
    {
        auto g = std::lock_guard{ m_mx };
        m_tracked_keys.clear();
    }

    void monitor( KeyIndex key, KeyState lookfor )
    {
        auto g = std::lock_guard{ m_mx };
        m_tracked_keys.insert_or_assign( key, TrackedKeyInfo{ KeyState::e_idle, lookfor } );
    }

    void monitor( ranges::range auto&& range )
    {
        auto g = std::lock_guard{ m_mx };
        for ( auto&& elem : range )
        {
            auto [ key, lookfor ] = elem;
            m_tracked_keys.insert_or_assign( key, TrackedKeyInfo{ KeyState::e_idle, lookfor } );
        }
    }

  public:
    using PollResult = std::unordered_map<KeyIndex, KeyState>;
    PollResult poll()
    {
        auto g = std::lock_guard{ m_mx };

        auto filtered = ranges::views::filter( m_tracked_keys, []( auto&& info ) {
            auto [ current, lookfor ] = info.second;
            return current == lookfor;
        } );

        PollResult result;
        for ( auto& info : filtered )
        {
            auto key = info.first;
            auto& [ current, lookfor ] = info.second;
            result.emplace( key, current );

            if ( current == KeyState::e_clicked )
            {
                current = KeyState::e_idle;
            }
        }

        return result;
    }
};

}; // namespace input::glfw