#pragma once

#include <concepts>
#include <fstream>
#include <functional>
#include <type_traits>

namespace utils
{

template <typename T>
concept Enumeration = std::is_enum_v<T>;

// clang-format off

template <typename T, typename D>
concept ExplicitlyConvertibleTo = requires ( T source ) {
    { static_cast<D>( source ) };
};

template <typename T> concept Hashable = requires (T v) {
  { std::hash<T>{}(v) } -> std::integral;
};

template <typename T>
concept IsIFStream = requires (T stream) {
  { [] <typename CharType, typename Traits> (std::basic_ifstream<CharType, Traits> &) {} (stream) };
};

template <typename T>
concept IsOFStream = requires (T stream) {
  { [] <typename CharType, typename Traits> (std::basic_ofstream<CharType, Traits> &) {} (stream) };
};

template <typename T>
concept IsFStream = requires (std::remove_cvref_t<T> stream) {
  requires IsIFStream<T> || IsOFStream<T>;
};

// clang-format on

}; // namespace utils