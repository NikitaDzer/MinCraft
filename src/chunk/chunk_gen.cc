#include "chunk/chunk_gen.h"

#include <PerlinNoise.hpp>

#include <range/v3/algorithm/for_each.hpp>
#include <range/v3/view/cartesian_product.hpp>
#include <range/v3/view/iota.hpp>

#include <spdlog/spdlog.h>

#include <cstdlib>
#include <limits>
#include <random>

namespace chunk
{

namespace
{

void
perlinChunkGen( Chunk& chunk )
{
    static auto noise = siv::PerlinNoise{ std::random_device{} };

    const auto iota = ranges::views::iota( 0, Chunk::k_max_width_length );
    const auto pos = chunk.getPosition();

    spdlog::debug( "{}, {}", pos.x * Chunk::k_max_width_length, pos.y * Chunk::k_max_width_length );

    for ( auto&& [ local_x, local_y ] : ranges::views::cartesian_product( iota, iota ) )
    {
        float absoulute_x = pos.x * Chunk::k_max_width_length + local_x;
        float absoulute_y = pos.y * Chunk::k_max_width_length + local_y;

        for ( auto z : ranges::views::iota( 0, Chunk::k_max_height ) )
        {
            float rel_height = static_cast<float>( z ) / Chunk::k_max_height;

            BlockID block = BlockID::k_none;
            auto frequency = 0.03f;

            auto dirt_level =
                noise.octave2D_01( frequency * absoulute_x, frequency * absoulute_y, 4 ) * 0.075f + 0.075f;

            auto stone_level =
                noise.octave2D_01( frequency * absoulute_x, frequency * absoulute_y, 2 ) * 0.125f + 0.05f;

            if ( rel_height < stone_level )
            {
                block = BlockID::k_stone;
            } else if ( rel_height < dirt_level )
            {
                block = BlockID::k_dirt;
            }

            chunk.at( local_x, local_y, z ) = block;
        }
    }
}

} // namespace

void
simpleChunkGen( Chunk& chunk_to_gen )
{
    perlinChunkGen( chunk_to_gen );
} // simpleChunkGen

}; // namespace chunk
