#include "chunk/chunk_gen.h"
#include <limits>

namespace chunk
{
void
simpleChunkGen( Chunk& chunk_to_gen )
{
    for ( uint8_t x = 0; x < Chunk::k_max_width_length; x++ )
    {
        for ( uint8_t y = 0; y < Chunk::k_max_width_length; y++ )
        {
            for ( uint8_t z = 0;; z++ )
            {
                // For higher half place none, for lower half place dirt
                if ( z >= std::numeric_limits<uint8_t>::max() / 2 )
                {
                    chunk_to_gen[ pos::LocalBlockPos{ x, y, z } ] = BlockID::k_none;
                } else
                {
                    chunk_to_gen[ pos::LocalBlockPos{ x, y, z } ] = BlockID::k_dirt;
                }

                // because z are uint8_t and in scope [0, 255] you need to "manually" break cycle
                if ( z == std::numeric_limits<uint8_t>::max() )
                {
                    break;
                }
            }
        }
    }
}
}; // namespace chunk
