#pragma once

#include "chunk/chunk.h"
#include <memory>

namespace chunk
{

/**
 * Class containing hash function for std::unordered_map
 */
struct ChunkHasher
{
    std::size_t operator()( const pos::ChunkPos& chunk_pos ) const
    {
        std::hash<int64_t> hasher{};
        return hasher( int64_t( chunk_pos ) );
    }
};

/**
 * Class that contains all block ides and manage the chunks
 */

class Region
{
  public:
    using ChunkMap = std::unordered_map<pos::ChunkPos, Chunk, ChunkHasher>;
    using BlockIDPtr = std::unique_ptr<BlockID[]>;

  public:
    Region( const Region& ) = delete;
    Region( Region&& ) = delete;

    Region& operator=( const Region& ) = delete;
    Region& operator=( Region&& ) = delete;
    ~Region() = default;

    static Region& getRef()
    {
        static Region singleton_region{ { 0, 0 } };
        return singleton_region;
    }

    Chunk& getChunk( const pos::ChunkPos& pos );
    pos::ChunkPos& getBeginPos() { return m_begin_pos; }
    pos::ChunkPos getBeginPos() const { return m_begin_pos; }

    // Change the region begin position ( the new_begin is the chunk, where the player is )
    void changeBeginPos( const pos::ChunkPos& new_begin );

  public:
    // Max chunks that can be viewed by the player
    static constexpr auto k_render_distance = 12;
    // the region is a square with the side equal to 2 * k_render_distance + 1
    static constexpr auto k_chunks_count = ( 2 * k_render_distance + 1 ) * ( 2 * k_render_distance + 1 );
    static constexpr auto k_blocks_count = k_chunks_count * Chunk::k_block_count;

  private:
    /// [krisszzz] This constructor should be changed in the future
    /// because of chunk serialization ( working with file, etc.. )
    Region( const pos::ChunkPos& begin_pos );

  private:
    pos::ChunkPos m_begin_pos;
    ChunkMap m_chunks;
    // std::unordered_map<pos::ChunkPos, BlockID*> m_chunks;
    BlockIDPtr m_block_ides;
};

}; // namespace chunk
