#pragma once

#include <spdlog/fmt/bundled/core.h>

#include "range/v3/view/transform.hpp"
#include <range/v3/algorithm/copy.hpp>
#include <range/v3/range.hpp>

#include <utility>

namespace utility
{

// Replacement for yet unimplemented C++23 to_underlying
constexpr auto
toUnderlying( auto val ) -> std::underlying_type_t<decltype( val )>
{
    return static_cast<std::underlying_type_t<decltype( val )>>( val );
}

// clang-format off
template <typename T>
concept convertibleToCStrRange = requires ()
{
  requires ranges::range<T>;
  requires requires (ranges::range_value_t<T> a) {{ a.c_str() } -> std::convertible_to<const char*>;}; 
};
// clang-format on

template <typename T>
constexpr decltype( auto )
convertToCStrVector( T&& range )
    requires ranges::contiguous_range<T> && std::convertible_to<ranges::range_value_t<T>, const char*>
{
    return range | ranges::views::transform( []( auto&& a ) { return static_cast<const char*>( a ); } ) |
        ranges::to_vector;
}

template <typename T>
constexpr auto
convertToCStrVector( T&& range )
    requires convertibleToCStrRange<T>
{
    std::vector<const char*> raw_strings;
    if constexpr ( ranges::random_access_range<T> )
    {
        raw_strings.reserve( range.size() );
    }

    return range | ranges::views::transform( []( auto&& a ) { return a.c_str(); } ) | ranges::to_vector;
}

}; // namespace utility
