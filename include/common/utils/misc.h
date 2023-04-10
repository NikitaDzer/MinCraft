#pragma once

#include "utils/concepts.h"

#include <cassert>
#include <concepts>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <type_traits>
#include <unordered_map>

namespace utils
{

template <Hashable T>
constexpr void
hashCombine( std::size_t& seed, const T& v )
{
    std::hash<T> hasher;
    seed ^= hasher( v ) + 0x9e3779b9 + ( seed << 6 ) + ( seed >> 2 );
}

// Convert to bool
constexpr bool
toBool( const ExplicitlyConvertibleTo<bool> auto& arg )
{
    return static_cast<bool>( arg );
}

// Replacement for yet unimplemented C++23 to_underlying
template <Enumeration T>
constexpr auto
toUnderlying( const T& val )
{
    return static_cast<std::underlying_type_t<T>>( val );
}

template <typename Key, typename Stored, typename Hash = std::hash<Key>, typename EqualTo = std::equal_to<Key>>
class UniquePointerMap
{
  public:
    // Return pointer because it can be nullptr
    Stored* lookup( const Key& key ) const
    {
        auto g = std::lock_guard{ m_mx };

        if ( auto found = m_map.find( key ); found != m_map.end() )
        {
            auto& unique = found->second;
            assert( unique && "Class invariant broken" );
            return unique.get();
        }

        return nullptr;
    }

    template <typename... Ts> Stored& emplaceOrAssign( const Key& key, Ts&&... args )
    {
        auto g = std::lock_guard{ m_mx };

        auto unique = std::make_unique<Stored>( std::forward<Ts>( args )... );
        auto& ref = *unique;
        m_map.insert_or_assign( key, std::move( unique ) );

        return ref;
    }

  private:
    std::unordered_map<Key, std::unique_ptr<Stored>, Hash, EqualTo> m_map;
    mutable std::mutex m_mx;
};

} // namespace utils