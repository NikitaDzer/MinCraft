#pragma once

#include <compare>
#include <cstdint>

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

}; // namespace pos
