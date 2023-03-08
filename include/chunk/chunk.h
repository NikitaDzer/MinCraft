#pragma once

#include "common/pos.h"
#include <cassert>
#include <cstdint>
#include <unordered_map>

namespace chunk
{

enum class BlockID : uint16_t
{
#define REGISTER_BLOCK( block_name, value ) k_##block_name = value,

#include "common/block_id.macro.h"

#undef REGISTER_BLOCK
};

class Chunk
{
  public:
    Chunk( BlockID* chunk_begin )
        : m_chunk_begin( chunk_begin ){};

    BlockID operator[]( int indx ) const
    {
        assert( indx < k_block_count );
        return m_chunk_begin[ indx ];
    }

    BlockID operator[]( const pos::LocalBlockPos& pos ) const
    {
        assert( pos.getX() < k_max_width_length && pos.getY() < k_max_width_length );
        return m_chunk_begin[ k_max_width_length * k_max_height * pos.getX() + k_max_height * pos.getY() + pos.getZ() ];
    }

    BlockID& operator[]( int indx )
    {
        assert( indx < k_block_count );
        return m_chunk_begin[ indx ];
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
};

}; // namespace chunk
