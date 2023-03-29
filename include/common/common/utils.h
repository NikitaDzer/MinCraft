#pragma once

#include "range/v3/view/transform.hpp"
#include <range/v3/algorithm/contains.hpp>
#include <range/v3/range/concepts.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/filter.hpp>

#include <utility>

namespace utils
{

// Algorithm to find all elements from `find` range of type FindRange that are not contained in `all`. Return a vector
// of type range_value_t<FindRange>;
template <typename AllRange, typename FindRange, typename Proj>
auto
findAllMissing( AllRange&& all, FindRange&& find, Proj proj )
{
    return ranges::views::all( find ) |
        ranges::views::filter( [ &all, &proj ]( auto&& elem ) { return !ranges::contains( all, elem, proj ); } ) |
        ranges::to_vector;
}

template <typename T>
concept Enumeration = std::is_enum_v<T>;

// Replacement for yet unimplemented C++23 to_underlying
template <Enumeration T>
constexpr auto
toUnderlying( const T& val )
{
    return static_cast<std::underlying_type_t<T>>( val );
}

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
template <typename T> concept Hashable = requires (T v) {
  { std::hash<T>{}(v) } -> std::integral;
}; // clang-format on

template <Hashable T>
void
hashCombine( std::size_t& seed, const T& v )
{
    std::hash<T> hasher;
    seed ^= hasher( v ) + 0x9e3779b9 + ( seed << 6 ) + ( seed >> 2 );
}

// clang-format off
template <typename T, typename D>
concept CoercibleTo = requires ( T source ) {
    { static_cast<D>( source ) };
};
// clang-format on

template <CoercibleTo<bool> T>
bool
toBool( T&& arg )
{
    return static_cast<bool>( std::forward<T>( arg ) );
}

}; // namespace utils
