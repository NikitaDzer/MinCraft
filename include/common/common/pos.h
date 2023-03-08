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
    ChunkPos( int x = 0, int y = 0 )
        : m_x( x ),
          m_y( y ){};

    int& getX() { return m_x; }
    int& getY() { return m_y; }

    int getX() const { return m_x; }
    int getY() const { return m_y; }

    auto operator<=>( const ChunkPos& ) const = default;

    explicit operator int64_t() const
    {
        return ( ( reinterpret_cast<const int64_t&>( m_y ) << 32 ) | reinterpret_cast<const int64_t&>( m_x ) );
    }

  private:
    int m_x;
    int m_y;
};

/**
 * Class that used to find block ides in Region
 * Note that BlockPos X and Y coordinate has nothing to do with
 * ChunkPos X and Y coordinate
 */

class BlockPos
{
  public:
    BlockPos( int x = 0, int y = 0, uint8_t z = 0 )
        : m_x( x ),
          m_y( y ),
          m_z( z ){};

    int& getX() { return m_x; }
    int& getY() { return m_y; }
    uint8_t& getZ() { return m_z; }

    int getX() const { return m_x; }
    int getY() const { return m_y; }
    uint8_t getZ() const { return m_z; }

  private:
    int m_x;
    int m_y;
    uint8_t m_z; /// The vertical coordinate Z start from 0 until 255
};

/**
 * Class describing position of block in the Chunk
 */

class LocalBlockPos
{
  public:
    LocalBlockPos( uint8_t x = 0, uint8_t y = 0, uint8_t z = 0 )
        : m_x( x ),
          m_y( y ),
          m_z( z ){};

    uint8_t& getX() { return m_x; }
    uint8_t& getY() { return m_y; }
    uint8_t& getZ() { return m_z; }

    uint8_t getX() const { return m_x; }
    uint8_t getY() const { return m_y; }
    uint8_t getZ() const { return m_z; }

    // Convert LocalBlockPos to BlockPos
    // The position on the chunk where the block is saved required
    BlockPos toBlockPos( const ChunkPos& pos );

  private:
    uint8_t m_x;
    uint8_t m_y;
    uint8_t m_z;
};

}; // namespace pos
