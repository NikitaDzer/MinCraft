#pragma once

#include "range/v3/view/transform.hpp"
#include <range/v3/algorithm/contains.hpp>
#include <range/v3/range/concepts.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/filter.hpp>

#include <utility>

namespace utils
{

// clang-format off
template <typename T>
concept convertibleToCStrRange = requires ()
{
  requires ranges::range<T>;
  requires requires (ranges::range_value_t<T> a) {{ a.c_str() } -> std::convertible_to<const char*>;}; 
};
// clang-format on

using CStrVector = std::vector<const char*>;

// Overload set with concepts. If the range elements can be coerced into const char *, then we assume they are c-style
// null-terminated strings and project those into a vector
template <ranges::range T>
constexpr CStrVector
convertToCStrVector( T&& range )
    requires std::convertible_to<ranges::range_value_t<T>, const char*>
{
    return range | ranges::views::transform( []( auto&& a ) { return static_cast<const char*>( a ); } ) |
        ranges::to_vector; // Eager conversion to std::vector<const char *>
}

// Convert a range of std::string-like objects into a vector of const char * to null-terminated strings.
// Range member type should have a .c_str member function. Note that std::string_view can't be used because it does not
// guarantee that the underlying buffer is null-terminated.
template <convertibleToCStrRange T>
constexpr CStrVector
convertToCStrVector( T&& range )
{
    return range | ranges::views::transform( []( auto&& a ) { return a.c_str(); } ) | ranges::to_vector;
}

// clang-format off

}; // namespace utils
