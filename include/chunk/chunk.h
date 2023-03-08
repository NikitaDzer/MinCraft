#pragma once

#include "position/position.h"
#include <cassert>
#include <cstdint>
#include <unordered_map>

namespace chunk
{

enum class BlockID : uint16_t
{
#define REGISTER_BLOCK( block_name, ... ) k_##block_name,

#include "chunk/block_id.inc"

#undef REGISTER_BLOCK
}; // enum class BlockID

class Chunk
{
  public:
    Chunk( BlockID* chunk_begin )
        : m_chunk_begin( chunk_begin ){};

    const BlockID& operator[]( int index ) const
    {
        assert( indx < k_block_count );
        return m_chunk_begin[ index ];
    }

    const BlockID& operator[]( const pos::LocalBlockPos& pos ) const
    {
        assert( pos.getX() < k_max_width_length && pos.getY() < k_max_width_length );
        return m_chunk_begin[ k_max_width_length * k_max_height * pos.getX() + k_max_height * pos.getY() + pos.getZ() ];
    }

    BlockID& operator[]( int index )
    {
        assert( indx < k_block_count );
        return m_chunk_begin[ index ];
    }

    BlockID& operator[]( const pos::LocalBlockPos& pos )
    {
        assert( pos.getX() < k_max_width_length && pos.getY() < k_max_width_length );
        return m_chunk_begin[ k_max_width_length * k_max_height * pos.getX() + k_max_height * pos.getY() + pos.getZ() ];
    }

    BlockID* getRawBlockIDPtr() { return m_chunk_begin; }

  public:
    static constexpr auto k_max_height = 256;
    static constexpr auto k_max_width_length = 16;
    static constexpr auto k_block_count = k_max_width_length * k_max_width_length * k_max_height;

  private:
    // Pointer to the raw block ides in the array
    BlockID* m_chunk_begin;
}; // class Chunk
}; // namespace chunk
