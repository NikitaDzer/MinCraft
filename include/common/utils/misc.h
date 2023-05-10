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

} // namespace utils