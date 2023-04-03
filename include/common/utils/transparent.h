/*
 * Transparent comparators and hash functors for string-like objects, which are convertible to std::string_view. These
 * comparators and hashes allow to address assosiative containers like std::unordered_map without extra copies. For more
 * information look at: https://en.cppreference.com/w/cpp/container/unordered_map/find
 *
 */

#pragma once

#include "utils/concepts.h"

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace utils
{

struct StringEqual
{
    using is_transparent = void;
    bool operator()(
        const ExplicitlyConvertibleTo<std::string_view> auto& lhs,
        const ExplicitlyConvertibleTo<std::string_view> auto& rhs ) const
    {
        return static_cast<std::string_view>( lhs ) == static_cast<std::string_view>( rhs );
    }
};

struct StringHash
{
    using is_transparent = StringEqual;
    bool operator()( const ExplicitlyConvertibleTo<std::string_view> auto& val ) const
    {
        return std::hash<std::string_view>{}( val );
    }
};

template <typename Mapped> using StringUnorderedMap = std::unordered_map<std::string, Mapped, StringHash, StringEqual>;

} // namespace utils