#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

namespace utils
{

constexpr auto
hexToRGBA( uint32_t hex )
{
    return std::to_array(
        { ( static_cast<float>( ( hex >> 24 ) & 0xff ) / 255.0f ),
          ( static_cast<float>( ( hex >> 16 ) & 0xff ) / 255.0f ),
          ( static_cast<float>( ( hex >> 8 ) & 0xff ) / 255.0f ),
          ( static_cast<float>( ( hex >> 0 ) & 0xff ) / 255.0f ) } );
}

}; // namespace utils