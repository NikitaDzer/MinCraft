#pragma once

#include "utils/concepts.h"

#include <bit>
#include <cstdint>
#include <functional>

namespace utils
{

template <Hashable T>
void
hashCombine( std::size_t& seed, const T& v )
{
    std::hash<T> hasher;
    seed ^= hasher( v ) + 0x9e3779b9 + ( seed << 6 ) + ( seed >> 2 );
}

// Convert to bool
template <ExplicitlyConvertibleTo<bool> T>
bool
toBool( const T& arg )
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

} // namespace utils