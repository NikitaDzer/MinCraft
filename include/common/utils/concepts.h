#pragma once

#include <concepts>
#include <functional>
#include <type_traits>

namespace utils
{

template <typename T>
concept Enumeration = std::is_enum_v<T>;

// clang-format off
template <typename T, typename D>
concept CoercibleTo = requires ( T source ) {
    { static_cast<D>( source ) };
};

template <typename T> concept Hashable = requires (T v) {
  { std::hash<T>{}(v) } -> std::integral;
}; // clang-format on

}; // namespace utils