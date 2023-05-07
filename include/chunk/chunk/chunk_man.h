#pragma once

#include "chunk/chunk.h"
#include "utils/misc.h"

#include <memory>

/**
 * Class containing hash function for std::unordered_map
 */
namespace std
{

template <> struct hash<pos::ChunkPos>
{
    size_t operator()( const pos::ChunkPos& chunk_pos ) const
    {
        std::hash<decltype( chunk_pos.x )> hasher;
        size_t seed = 0;
        utils::hashCombine( seed, hasher( chunk_pos.x ) );
        utils::hashCombine( seed, hasher( chunk_pos.y ) );
        return seed;
    }
}; // struct ChunkHasher

} // namespace std

namespace chunk
{

/**
 * Class that contains all block ides and manage the chunks
 */

class ChunkMan
{

  public:
    // Max chunks that can be viewed by the player
    static constexpr auto k_render_distance = 10;
    // the region is a square with the side equal to 2 * k_render_distance + 1
    static constexpr auto k_chunks_count = ( 2 * k_render_distance + 1 ) * ( 2 * k_render_distance + 1 );
    static constexpr auto k_blocks_count = k_chunks_count * Chunk::k_block_count;

  public:
    using ChunkMap = std::unordered_map<pos::ChunkPos, Chunk>;
    using BlockIDPtr = std::unique_ptr<std::array<BlockID, k_blocks_count>>;

  public:
    ChunkMan( const ChunkMan& ) = delete;
    ChunkMan( ChunkMan&& ) = delete;

    ChunkMan& operator=( const ChunkMan& ) = delete;
    ChunkMan& operator=( ChunkMan&& ) = delete;
    ~ChunkMan() = default;

    static ChunkMan& getRef()
    {
        static ChunkMan singleton_region{ { 0, 0 } };
        return singleton_region;
    } // ChunkMan::getRef

    Chunk& getChunk( const pos::ChunkPos& pos );
    pos::ChunkPos& getOriginPos() { return m_origin_pos; }
    const pos::ChunkPos& getOriginPos() const { return m_origin_pos; }

    // Change the region origin position ( the new_origin is the chunk, where the player is )
    void changeOriginPos( const pos::ChunkPos& new_origin );

  private:
    /// [krisszzz] This constructor should be changed in the future
    /// because of chunk serialization ( working with file, etc.. )
    ChunkMan( const pos::ChunkPos& origin_pos );

  private:
    pos::ChunkPos m_origin_pos;
    ChunkMap m_chunks;
    BlockIDPtr m_block_ides;
}; // class ChunkMan

}; // namespace chunk
