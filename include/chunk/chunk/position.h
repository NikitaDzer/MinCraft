#pragma once

#include <compare>
#include <cstdint>

namespace pos
{

/**
 * Class that used to find chunks in Region
 */

class ChunkPos
{
  public:
    ChunkPos( int x_par = 0, int y_par = 0 )
        : x( x_par ),
          y( y_par ){};

  public:
    int x;
    int y;
}; // class ChunkPos

// [krisszzz]: Inline here is meanigful, otherwise you will get multiple definition
// To be honest the function definition should be in position.cc, but creating source
// file only for this purpose is not necessary yet
inline bool
operator==( const ChunkPos& lhs, const ChunkPos& rhs )
{
    return ( lhs.x == rhs.x && lhs.y == rhs.y );
}

inline bool
operator!=( const ChunkPos& lhs, const ChunkPos& rhs )
{
    return !( lhs == rhs );
}

/**
 * Class that used to find block ides in Region
 * Note that BlockPos X and Y coordinate has nothing to do with
 * ChunkPos X and Y coordinate
 */

class BlockPos
{
  public:
    BlockPos( int x_par = 0, int y_par = 0, uint8_t z_par = 0 )
        : x( x_par ),
          y( y_par ),
          z( z_par ){};

  public:
    int x;
    int y;
    uint8_t z; /// The vertical coordinate Z start from 0 until 255

}; // class BlockPos

/**
 * Class describing position of block in the Chunk
 */

class LocalBlockPos
{
  public:
    LocalBlockPos( uint8_t x_par = 0, uint8_t y_par = 0, uint8_t z_par = 0 )
        : x( x_par ),
          y( y_par ),
          z( z_par ){};

    // Convert LocalBlockPos to BlockPos
    // The position on the chunk where the block is saved required
    // Not implemented
    BlockPos toBlockPos( const ChunkPos& pos );

  public:
    uint8_t x;
    uint8_t y;
    uint8_t z;
}; // class LocalBlockPos

}; // namespace pos
