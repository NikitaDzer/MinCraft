#pragma once

#include <compare>
#include <cstdint>

#include "common/glm_include.h"

namespace pos
{

/**
 * Class that used to find chunks in ChunkMan
 */

class ChunkPos
{
  public:
    constexpr ChunkPos( int x_par = 0, int y_par = 0 )
        : x( x_par ),
          y( y_par ){};

    friend bool operator==( const ChunkPos& lhs, const ChunkPos& rhs ) = default;

    ChunkPos& operator+=( ChunkPos rhs )
    {
        x += rhs.x;
        y += rhs.y;

        return *this;
    }

    ChunkPos operator+( const ChunkPos rhs ) const
    {
        ChunkPos tmp = *this;
        tmp += rhs;
        return tmp;
    }

    ChunkPos& operator-=( ChunkPos rhs )
    {
        x -= rhs.x;
        y -= rhs.y;

        return *this;
    }

    ChunkPos operator-( const ChunkPos rhs ) const
    {
        ChunkPos tmp = *this;
        tmp -= rhs;
        return tmp;
    }

  public:
    int x;
    int y;
}; // class ChunkPos

/*
 * class that used to indexing by local coordinates in array of blocks
 * that should be rendered
 */

union RenderAreaBlockPos {
  public:
    constexpr RenderAreaBlockPos( uint16_t local_x, uint16_t local_y, uint16_t local_z )
        : x( local_x ),
          y( local_y ),
          z( local_z ){};

    RenderAreaBlockPos& operator+=( RenderAreaBlockPos rhs )
    {
        x += rhs.x;
        y += rhs.y;
        z += rhs.z;

        return *this;
    }

    RenderAreaBlockPos operator+( const RenderAreaBlockPos rhs ) const
    {
        RenderAreaBlockPos tmp = *this;
        tmp += rhs;
        return tmp;
    }

    RenderAreaBlockPos& operator-=( const RenderAreaBlockPos rhs )
    {
        x -= rhs.x;
        y -= rhs.y;
        z -= rhs.z;

        return *this;
    }

    RenderAreaBlockPos operator-( const RenderAreaBlockPos rhs ) const
    {
        RenderAreaBlockPos tmp = *this;
        tmp -= rhs;
        return tmp;
    }

  public:
    constexpr static auto max_x = 11;
    constexpr static auto max_y = 11;
    constexpr static auto max_z = 10;
    // coordinate system starting right corner of
    // render area
    struct __attribute__( ( packed ) )
    {
        uint16_t x : max_x;
        uint16_t y : max_y;
        uint16_t z : max_z;
    };

    uint32_t combined;
};

static_assert( sizeof( RenderAreaBlockPos ) == sizeof( uint32_t ) );

}; // namespace pos
