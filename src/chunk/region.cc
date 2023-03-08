#include "chunk/region.h"

namespace chunk
{

Region::Region( const pos::ChunkPos& begin_pos )
    : m_block_ides( std::make_unique_for_overwrite<BlockID[]>( k_blocks_count ) ),
      m_begin_pos( begin_pos )
{
    m_chunks.reserve( k_chunks_count );
    auto raw_block_ides_ptr = m_block_ides.get();

    auto min_x = begin_pos.getX() - k_render_distance;
    auto max_x = begin_pos.getX() + k_render_distance;

    auto min_y = begin_pos.getX() - k_render_distance;
    auto max_y = begin_pos.getY() + k_render_distance;

    for ( auto x = min_x; x <= max_x; x++ )
    {
        for ( auto y = min_y; y <= max_y; y++ )
        {
            m_chunks.emplace( pos::ChunkPos{ x, y }, raw_block_ides_ptr );
            raw_block_ides_ptr += Chunk::k_block_count;
        }
    }
};

Chunk&
Region::getChunk( const pos::ChunkPos& pos )
{
    assert(
        abs( pos.getX() - m_begin_pos.getX() ) <= k_render_distance &&
        abs( pos.getY() - m_begin_pos.getY() ) <= k_render_distance );

    auto res = m_chunks.find( pos );

    /// [krisszzz]: In the future should use for example std::optional
    assert( res != m_chunks.end() );

    return res->second;
}

void
Region::changeBeginPos( const pos::ChunkPos& new_begin )
{
    constexpr auto chunk_distance_diff = 1;

    auto raw_block_ides_ptr = m_block_ides.get();
    pos::ChunkPos player_direction{ new_begin.getX() - m_begin_pos.getX(), new_begin.getY() - m_begin_pos.getY() };

    // At once the player position should be changed only by 1 chunk
    assert(
        abs( player_direction.getX() ) <= chunk_distance_diff &&
        abs( player_direction.getY() ) <= chunk_distance_diff );

    // Replace old chunks with new
    if ( player_direction.getY() != 0 )
    {
        auto old_chunks_y = 0;
        auto new_chunks_y = 0;

        if ( player_direction.getY() > 0 )
        {
            // Old chunks that was "backward" in player render distance
            // New chunks that "forward" in player render distance
            old_chunks_y = m_begin_pos.getY() - k_render_distance;
            new_chunks_y = m_begin_pos.getY() + k_render_distance + chunk_distance_diff;
        } else
        {
            // Old chunks that was "forward" in player render distance
            // New chunks that "backward" in player render distance
            old_chunks_y = m_begin_pos.getY() + k_render_distance;
            new_chunks_y = m_begin_pos.getY() - k_render_distance - chunk_distance_diff;
        }

        auto min_x = m_begin_pos.getX() - k_render_distance;
        auto max_x = m_begin_pos.getX() + k_render_distance;

        for ( auto x = min_x; x <= max_x; x++ )
        {
            auto extracted_node = m_chunks.extract( pos::ChunkPos{ x, old_chunks_y } );
            extracted_node.key() = pos::ChunkPos{ x, new_chunks_y };
            m_chunks.insert( std::move( extracted_node ) );

            auto extracted_chunk = extracted_node.mapped();

            // Generate new chunk ( manipalation with extracted.mapped() )
            // Not implemented yet

            for ( int i = 0; i < Chunk::k_block_count; i++ )
            {
                extracted_chunk[ i ] = BlockID::k_none;
            }
        }
    } else if ( player_direction.getX() != 0 )
    {
        auto old_chunks_x = 0;
        auto new_chunks_x = 0;

        if ( player_direction.getX() > 0 )
        {
            // Old chunks that was "left" in player render distance
            // New chunks that "right" in player render distance
            old_chunks_x = m_begin_pos.getX() - k_render_distance;
            new_chunks_x = m_begin_pos.getX() + k_render_distance + chunk_distance_diff;
        } else
        {
            // Old chunks that was "right" in player render distance
            // New chunks that "left" in player render distance
            old_chunks_x = m_begin_pos.getX() + k_render_distance;
            new_chunks_x = m_begin_pos.getX() - k_render_distance - chunk_distance_diff;
        }

        auto min_y = m_begin_pos.getY() - k_render_distance;
        auto max_y = m_begin_pos.getY() + k_render_distance;

        for ( auto y = min_y; y <= max_y; y++ )
        {
            auto extracted_node = m_chunks.extract( pos::ChunkPos{ old_chunks_x, y } );
            extracted_node.key() = pos::ChunkPos{ new_chunks_x, y };
            m_chunks.insert( std::move( extracted_node ) );

            auto extracted_chunk = extracted_node.mapped();

            for ( int i = 0; i < Chunk::k_block_count; i++ )
            {
                extracted_chunk[ i ] = BlockID::k_none;
            }
        }
    }

    m_begin_pos = new_begin;
}

}; // namespace chunk
