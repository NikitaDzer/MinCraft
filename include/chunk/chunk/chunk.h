#pragma once

#include "chunk/position.h"

#include "utils/misc.h"
#include <cassert>
#include <cstdint>
#include <unordered_map>

namespace chunk
{

enum class BlockID : uint16_t
{

#define REGISTER_BLOCK( block_name, ... ) k_##block_name,
#include "detail/block_id.inc"
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

    const BlockID& at( uint8_t x, uint8_t y, uint8_t z ) const&
    {
        assert( x < k_max_width_length && y < k_max_width_length );
        return m_chunk_begin[ k_max_width_length * k_max_height * x + k_max_height * y + z ];
    }

    BlockID& at( uint8_t x, uint8_t y, uint8_t z ) &
    {
        assert( x < k_max_width_length && y < k_max_width_length );
        return m_chunk_begin[ k_max_width_length * k_max_height * x + k_max_height * y + z ];
    }

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
