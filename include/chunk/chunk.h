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

    const BlockID& operator[]( int index ) const&
    {
        assert( index < k_block_count );
        return m_chunk_begin[ index ];
    } // Chunk::operator[] const

    const BlockID& operator[]( const pos::LocalBlockPos& pos ) const&
    {
        assert( pos.x < k_max_width_length && pos.y < k_max_width_length );
        return m_chunk_begin[ k_max_width_length * k_max_height * pos.x + k_max_height * pos.y + pos.z ];
    } // Chunk::operator[] const

    BlockID& operator[]( int index ) &
    {
        assert( index < k_block_count );
        return m_chunk_begin[ index ];
    } // Chunk::operator[]

    BlockID& operator[]( const pos::LocalBlockPos& pos ) &
    {
        assert( pos.x < k_max_width_length && pos.y < k_max_width_length );
        return m_chunk_begin[ k_max_width_length * k_max_height * pos.x + k_max_height * pos.y + pos.z ];
    } // Chunk::operator[]

    BlockID& getChunkBegin() & { return m_chunk_begin[ 0 ]; }

  public:
    static constexpr auto k_max_height = 256;
    static constexpr auto k_max_width_length = 16;
    static constexpr auto k_block_count = k_max_width_length * k_max_width_length * k_max_height;

  private:
    // Pointer to the raw block ides in the array
    BlockID* m_chunk_begin;
}; // class Chunk

}; // namespace chunk
