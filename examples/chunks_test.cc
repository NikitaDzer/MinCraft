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
        region.changeOriginPos( { 0, i } );
    }

    auto get_chunk = region.getChunk( { -12, 100 } );
    auto block_id = get_chunk[ { 5, 5, 3 } ];

    auto test_cvt = int64_t( pos::ChunkPos{ -1, 100 } );

    printf( "%b\n", -1 );
    printf( "%b\n", 100 );
    printf( "%lb\n", test_cvt );

    return 0;
}
