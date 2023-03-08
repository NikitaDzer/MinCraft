#include "chunk/region.h"
#include <iostream>

int
main()
{
    // or decltype( auto )
    auto& region = chunk::Region::getRef();

    std::cout << "Render distance: " << chunk::Region::k_render_distance << std::endl;
    std::cout << "Chunks in Region: " << chunk::Region::k_chunks_count << std::endl;

    std::cout << "Allocated region size (in MegaBytes ): "
              << ( chunk::Region::k_blocks_count * sizeof( chunk::BlockID ) ) / ( 1024 * 1024 ) << std::endl;

    // Move "forward" ( along the Y axis )
    for ( int i = 0; i <= 100; i++ )
    {
        region.changeBeginPos( { 0, i } );
    }

    auto get_chunk = region.getChunk( { -12, 100 } );
    auto block_id = get_chunk[ { 5, 5, 3 } ];
}
