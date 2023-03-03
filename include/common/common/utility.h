#pragma once

#include <spdlog/fmt/bundled/core.h>

#include "range/v3/view/transform.hpp"
#include <range/v3/algorithm/contains.hpp>
#include <range/v3/algorithm/copy.hpp>
#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/range.hpp>

#include <utility>

namespace utility
{

// Algorithm to find all elements from `find` range of type FindRange that are not contained in `all`. Return a vector
// of type range_value_t<FindRange>;
template <typename AllRange, typename FindRange, typename Proj>
auto
findAllMissing( AllRange&& all, FindRange&& find, Proj proj )
{
    std::vector<typename ranges::range_value_t<std::remove_reference_t<FindRange>>> missing;
    ranges::for_each( find, [ &all, &proj, &missing ]( auto&& elem ) {
        if ( !ranges::contains( all, elem, proj ) )
            missing.push_back( elem );
    } );
    return missing;
}

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

// Overload set with concepts. If the range elements can be coerced into const char *, then we assume they are c-style
// null-terminated strings and project those into a vector
template <typename T>
constexpr auto
convertToCStrVector( T&& range )
    requires ranges::contiguous_range<T> && std::convertible_to<ranges::range_value_t<T>, const char*>
{
    return range | ranges::views::transform( []( auto&& a ) { return static_cast<const char*>( a ); } ) |
        ranges::to_vector; // Eager conversion to std::vector<const char *>
}

// Convert a range of std::string-like objects into a vector of const char * to null-terminated strings.
// Range member type should have a .c_str member function. Note that std::string_view can't be used because it does not
// guarantee that the underlying buffer is null-terminated.
template <typename T>
constexpr auto
convertToCStrVector( T&& range )
    requires convertibleToCStrRange<T>
{
    std::vector<const char*> raw_strings;
    if constexpr ( ranges::random_access_range<T> ) // If a range is random access then it's possible to prereserve the
                                                    // space.
    {
        raw_strings.reserve( range.size() );
    }

    return range | ranges::views::transform( []( auto&& a ) { return a.c_str(); } ) | ranges::to_vector;
}

}; // namespace utility
