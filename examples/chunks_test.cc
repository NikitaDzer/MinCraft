#include "chunk/chunk_man.h"
#include "chunk/chunk_mesher.h"
#include <chrono>
#include <iostream>

int
main()
{
    // or decltype( auto )
    auto& chunk_man = chunk::ChunkMan::getRef();

    std::cout << "Render distance: " << chunk::ChunkMan::k_render_distance << std::endl;
    std::cout << "Chunks in ChunkMan: " << chunk::ChunkMan::k_chunks_count << std::endl;

    std::cout << "Allocated chunk_man size (in MegaBytes ): "
              << ( chunk::ChunkMan::k_blocks_count * sizeof( chunk::BlockID ) ) / ( 1024 * 1024 ) << std::endl;

    auto start_time = std::chrono::high_resolution_clock::now();

    chunk::ChunkMesher mesher{};
    mesher.startMesh();

    auto finish_time = std::chrono::high_resolution_clock::now();
    auto elapsed_time =
        std::chrono::duration<float, std::chrono::milliseconds::period>( finish_time - start_time ).count();

    std::cout << "[Meshing] vertices count: " << mesher.m_vertices.size() << " "
              << "indices count: " << mesher.m_indices.size() << "\n";

    std::cout << "[Meshing] elapsed time: " << elapsed_time << " millis\n";

    // Move "forward" ( along the Y axis )
    for ( int i = 0; i <= 100; i++ )
    {
        chunk_man.changeOriginPos( { 0, i } );
    }

    auto get_chunk = chunk_man.getChunk( { -12, 100 } );
    auto block_id = get_chunk.at( 5, 5, 3 );

    return 0;
}
