#include "chunk/chunk_gen.h"
#include "chunk/chunk_man.h"

namespace chunk
{

ChunkMan::ChunkMan( const pos::ChunkPos& origin_pos )
    : m_origin_pos( origin_pos ),
      m_block_ides( std::make_unique<std::array<BlockID, k_blocks_count>>() )
{
    m_chunks.reserve( k_chunks_count );
    auto* raw_block_ides_ptr = m_block_ides->data();

    auto min_x = origin_pos.x - k_render_distance;
    auto max_x = origin_pos.x + k_render_distance;

    auto min_y = origin_pos.x - k_render_distance;
    auto max_y = origin_pos.y + k_render_distance;

    for ( auto x = min_x; x <= max_x; x++ )
    {
        for ( auto y = min_y; y <= max_y; y++ )
        {
            m_chunks.emplace( pos::ChunkPos{ x, y }, raw_block_ides_ptr );
            Chunk tmp_chunk{ raw_block_ides_ptr };
            simpleChunkGen( tmp_chunk );
            raw_block_ides_ptr += Chunk::k_block_count;
        }
    }
}; // ChunkMan::ChunkMan

Chunk&
ChunkMan::getChunk( const pos::ChunkPos& pos )
{
    assert(
        std::abs( pos.x - m_origin_pos.x ) <= k_render_distance &&
        std::abs( pos.y - m_origin_pos.y ) <= k_render_distance );

    auto res = m_chunks.find( pos );

    /// [krisszzz]: In the future should use for example std::optional
    assert( res != m_chunks.end() );

    return res->second;
} // ChunkMan::getChunk

void
ChunkMan::changeOriginPos( const pos::ChunkPos& new_origin )
{
    constexpr auto chunk_distance_diff = 1;

    pos::ChunkPos player_direction{ new_origin.x - m_origin_pos.x, new_origin.y - m_origin_pos.y };

    // At once the player position should be changed only by 1 chunk
    assert(
        std::abs( player_direction.x ) <= chunk_distance_diff &&
        std::abs( player_direction.y ) <= chunk_distance_diff );

    // Replace old chunks with new
    if ( player_direction.y != 0 )
    {
        auto old_chunks_y = 0;
        auto new_chunks_y = 0;

        if ( player_direction.y > 0 )
        {
            // Old chunks that was "backward" in player render distance
            // New chunks that "forward" in player render distance
            old_chunks_y = m_origin_pos.y - k_render_distance;
            new_chunks_y = m_origin_pos.y + k_render_distance + chunk_distance_diff;
        } else
        {
            // Old chunks that was "forward" in player render distance
            // New chunks that "backward" in player render distance
            old_chunks_y = m_origin_pos.y + k_render_distance;
            new_chunks_y = m_origin_pos.y - k_render_distance - chunk_distance_diff;
        }

        auto min_x = m_origin_pos.x - k_render_distance;
        auto max_x = m_origin_pos.x + k_render_distance;

        for ( auto x = min_x; x <= max_x; x++ )
        {
            auto extracted_node = m_chunks.extract( pos::ChunkPos{ x, old_chunks_y } );
            auto extracted_chunk = extracted_node.mapped();
            extracted_node.key() = pos::ChunkPos{ x, new_chunks_y };
            m_chunks.insert( std::move( extracted_node ) );

            simpleChunkGen( extracted_chunk );
        }
    } else if ( player_direction.x != 0 )
    {
        auto old_chunks_x = 0;
        auto new_chunks_x = 0;

        if ( player_direction.x > 0 )
        {
            // Old chunks that was "left" in player render distance
            // New chunks that "right" in player render distance
            old_chunks_x = m_origin_pos.x - k_render_distance;
            new_chunks_x = m_origin_pos.x + k_render_distance + chunk_distance_diff;
        } else
        {
            // Old chunks that was "right" in player render distance
            // New chunks that "left" in player render distance
            old_chunks_x = m_origin_pos.x + k_render_distance;
            new_chunks_x = m_origin_pos.x - k_render_distance - chunk_distance_diff;
        }

        auto min_y = m_origin_pos.y - k_render_distance;
        auto max_y = m_origin_pos.y + k_render_distance;

        for ( auto y = min_y; y <= max_y; y++ )
        {
            auto extracted_node = m_chunks.extract( pos::ChunkPos{ old_chunks_x, y } );
            auto extracted_chunk = extracted_node.mapped();
            extracted_node.key() = pos::ChunkPos{ new_chunks_x, y };
            m_chunks.insert( std::move( extracted_node ) );

            simpleChunkGen( extracted_chunk );
        }
    }

    m_origin_pos = new_origin;
} // ChunkMan::changeOriginPos

}; // namespace chunk
