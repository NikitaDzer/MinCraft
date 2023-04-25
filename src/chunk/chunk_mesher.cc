#include "chunk/chunk_mesher.h"

namespace chunk
{

void
ChunkMesher::addFace( const auto& face_info )
{
    size_t vertex_count = m_vertices.size();

    m_vertices.emplace_back( face_info.v1, face_info.block_id, 0, 0 );
    m_vertices.emplace_back( face_info.v2, face_info.block_id, face_info.width, 0 );
    m_vertices.emplace_back( face_info.v3, face_info.block_id, 0, face_info.height );
    m_vertices.emplace_back( face_info.v4, face_info.block_id, face_info.width, face_info.height );

    // [krisszzzz]
    // change vertices order to backward ( from clockwise to counter-clockwise ) if
    // it is not a front face
    if ( face_info.is_front_face )
    {
        m_indices.push_back( vertex_count + 0 );
        m_indices.push_back( vertex_count + 1 );
        m_indices.push_back( vertex_count + 2 );
        m_indices.push_back( vertex_count + 1 );
        m_indices.push_back( vertex_count + 3 );
        m_indices.push_back( vertex_count + 2 );
    } else
    {
        m_indices.push_back( vertex_count + 2 );
        m_indices.push_back( vertex_count + 3 );
        m_indices.push_back( vertex_count + 1 );
        m_indices.push_back( vertex_count + 2 );
        m_indices.push_back( vertex_count + 1 );
        m_indices.push_back( vertex_count + 0 );
    }
} /* ChunkMesher::addFace */

auto
ChunkMesher::toRenderAreaBlockPos( uint16_t x, uint16_t y, uint16_t z, const pos::ChunkPos& chunk_pos ) const
{
    return RenderAreaBlockPos{
        static_cast<uint16_t>( ( chunk_pos.x - m_render_area_right.x ) * chunk::Chunk::k_max_width_length + x ),
        static_cast<uint16_t>( ( chunk_pos.y - m_render_area_right.y ) * chunk::Chunk::k_max_width_length + y ),
        z };
} /* ChunkMesher::toRenderAreaBlockPos */

void
ChunkMesher::meshRenderArea()
{

    auto&& chunk_man = ChunkMan::getRef();
    m_render_area_right =
        chunk_man.getOriginPos() - pos::ChunkPos{ chunk_man.k_render_distance, chunk_man.k_render_distance };

    for ( int x = -chunk_man.k_render_distance; x <= chunk_man.k_render_distance; x++ )
    {
        for ( int y = -chunk_man.k_render_distance; y <= chunk_man.k_render_distance; y++ )
        {
            auto&& chunk = chunk_man.getChunk( { x, y } );

            greedyMesh( { x, y }, chunk );
        }
    }
} /* ChunkMesher::meshRenderArea */

void
ChunkMesher::greedyMesh( const pos::ChunkPos& chunk_pos, const Chunk& chunk )
{
    // Sweep over each Axis ( X, Y, Z )
    for ( int dim = 0; dim < 3; dim++ )
    {
        constexpr int x = 0;
        constexpr int y = 1;
        constexpr int z = 2;
        constexpr int dim_count = 3;

        // width and height of cuboid
        int width = 0;
        int height = 0;

        // work with plane OUV ( U = { Y, Z, X }, V = { Z, X, Y } )
        int u = ( dim + 1 ) % dim_count;
        int v = ( dim + 2 ) % dim_count;

        // axis array
        std::array<int, 3> axis{};
        // direction array
        std::array<int, 3> dir{};

        // comparison map show the result of comparison block with the next block
        // with choosen direction
        static std::array<bool, Chunk::k_max_width_length * Chunk::k_max_height> cmp_map{};
        // normal map show the orientation of face ( back or front )
        static std::array<bool, Chunk::k_max_width_length * Chunk::k_max_height> normal_map{};
        // save the face of the block to draw
        static std::array<BlockID, Chunk::k_max_width_length * Chunk::k_max_height> face_map{};

        // define direction of comparison
        dir[ dim ] = 1;

        // limitation of iteration on the axis normal to the plane of OUV
        const int dir_limits = ( dim == z ) ? Chunk::k_max_height : Chunk::k_max_width_length;
        // U and V limitations
        const int u_limits = ( u == z ) ? Chunk::k_max_height : Chunk::k_max_width_length;
        const int v_limits = ( v == z ) ? Chunk::k_max_height : Chunk::k_max_width_length;
        // slice chunk with plane OUV
        for ( axis[ dim ] = -1; axis[ dim ] < dir_limits; )
        {
            int block_index = 0;

            for ( axis[ v ] = 0; axis[ v ] < v_limits; axis[ v ]++ )
            {
                for ( axis[ u ] = 0; axis[ u ] < u_limits; axis[ u ]++ )
                {
                    auto at_xyz = ( axis[ dim ] >= 0 ) ? chunk.at( axis[ x ], axis[ y ], axis[ z ] ) : BlockID::k_none;
                    auto at_xyz_dir = ( axis[ dim ] < dir_limits - 1 )
                        ? chunk.at( axis[ x ] + dir[ x ], axis[ y ] + dir[ y ], axis[ z ] + dir[ z ] )
                        : BlockID::k_none;

                    bool block_current = ( at_xyz == BlockID::k_none );
                    bool block_compare = ( at_xyz_dir == BlockID::k_none );

                    cmp_map[ block_index ] = ( block_current != block_compare );
                    face_map[ block_index ] = block_compare ? at_xyz : at_xyz_dir;
                    normal_map[ block_index ] = block_compare;

                    block_index++;
                }
            }

            block_index = 0;
            axis[ dim ]++;

            for ( int j = 0; j < v_limits; j++ )
            {
                for ( int i = 0; i < u_limits; )
                {
                    bool mask = cmp_map[ block_index ];
                    bool orientation = normal_map[ block_index ];
                    auto face_id = face_map[ block_index ];

                    if ( !mask )
                    {
                        block_index++;
                        i++;

                        continue;
                    }

                    for ( width = 1; i + width < u_limits; width++ )
                    {
                        if ( !cmp_map[ block_index + width ] || //
                             face_id != face_map[ block_index + width ] ||
                             orientation != normal_map[ block_index + width ] )
                        {
                            break;
                        }
                    }

                    bool done = false;

                    for ( height = 1; j + height < v_limits; height++ )
                    {
                        for ( int k = 0; k < width; k++ )
                        {
                            if ( !cmp_map[ block_index + k + height * u_limits ] ||
                                 face_id != face_map[ block_index + k + height * u_limits ] ||
                                 orientation != normal_map[ block_index + k + height * u_limits ] )
                            {
                                done = true;
                                break;
                            }
                        }

                        if ( done )
                        {
                            break;
                        }
                    }

                    axis[ u ] = i;
                    axis[ v ] = j;

                    std::array<int, 3> du{};
                    std::array<int, 3> dv{};

                    du[ u ] = width;
                    dv[ v ] = height;

                    FaceInfo face_info{
                        // clang-format off
                        .is_front_face = normal_map[ block_index ],
			.block_id = face_map[ block_index ],
			.width = width,
                        .height = height,
			.v1 = toRenderAreaBlockPos(
			    axis[ x ],
			    axis[ y ],
			    axis[ z ],
			    chunk_pos ),
                        .v2 = toRenderAreaBlockPos(
                            axis[ x ] + du[ x ],
                            axis[ y ] + du[ y ],
                            axis[ z ] + du[ z ],
                            chunk_pos ),
                        .v3 = toRenderAreaBlockPos(
                            axis[ x ] + dv[ x ],
                            axis[ y ] + dv[ y ],
                            axis[ z ] + dv[ z ],
                            chunk_pos ),
			.v4 = toRenderAreaBlockPos(
			    axis[ x ] + du[ x ] + dv[ x ],
			    axis[ y ] + du[ y ] + dv[ y ],
			    axis[ z ] + du[ z ] + dv[ z ],
			    chunk_pos )
                        // clang-format on
                    };

                    addFace( face_info );

                    // clear map
                    for ( int l = 0; l < height; l++ )
                    {
                        for ( int k = 0; k < width; k++ )
                        {
                            cmp_map[ block_index + k + l * u_limits ] = false;
                        }
                    }

                    i += width;
                    block_index += width;
                }
            }
        }
    }
} /* ChunkMesher::greedyMesh */

ChunkMesher::VertexInfo
ChunkMesher::getVertexInfo() const
{
    return VertexInfo{
        .binding_descr =
            {
                { { .binding = 0, //
                    .stride = sizeof( Vertex ),
                    .inputRate = vk::VertexInputRate::eVertex } } },

        .attribute_descr = {
            { { .location = 0, //
                .binding = 0,
                .format = vk::Format::eR32Uint,
                .offset = offsetof( Vertex, position ) },

              { .location = 1,
                .binding = 0,
                .format = vk::Format::eR32Uint,
                .offset = offsetof( Vertex, tex_descr ) } } } };
} /* ChunkMesher::getVertexInfo */

} // namespace chunk
