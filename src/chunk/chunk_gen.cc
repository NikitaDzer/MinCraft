#include "chunk/chunk_gen.h"
#include <cstdlib>
#include <limits>

namespace chunk
{
void
simpleChunkGen( Chunk& chunk_to_gen )
{
    for ( int x = 0; x < Chunk::k_max_width_length; x++ )
    {
        for ( int y = 0; y < Chunk::k_max_width_length; y++ )
        {
            for ( int z = 0; z < Chunk::k_max_height; z++ )
            {
                chunk_to_gen.at( x, y, z ) = static_cast<BlockID>( rand() % 3 );
            }
        }
    }
} // simpleChunkGen

}; // namespace chunk
