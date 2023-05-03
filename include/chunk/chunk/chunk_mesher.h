#include "chunk/chunk.h"
#include "chunk/chunk_man.h"
#include "common/vulkan_include.h"
#include <cstdlib>
#include <iostream>

namespace chunk
{

/* class that convert block id and position to an array of vertices */
class ChunkMesher
{
    /* These structures are part of realisition of ChunkMesher.
     * User has nothing to do with this structures, he only need information
     * about vertex type for Vulkan correct work.
     * This information is represented by VertexInfo structure.
     */
  private:
    /*
     * struct containing information about vertex texture coordinates ( u and v ) and
     * id of the block that this vertex is representing
     */
    struct __attribute__( ( packed ) ) VertexTextureDescr
    {
      private:
        constexpr static auto k_max_u_bit = 9;
        constexpr static auto k_max_v_bit = 9;
        constexpr static auto k_max_block_id_bit = 14;

      public:
        constexpr static auto k_max_u = ( 1 << k_max_u_bit ) - 1;
        constexpr static auto k_max_v = ( 1 << k_max_v_bit ) - 1;
        constexpr static auto k_max_block_id = ( 1 << k_max_block_id_bit ) - 1;

        static_assert( k_max_block_id >= utils::toUnderlying( BlockID::k_max ), "Cannot represent all block ides" );

        constexpr VertexTextureDescr( BlockID block_id, uint16_t u_par, uint16_t v_par )
            : u( u_par ),
              v( v_par ),
              block_id( utils::toUnderlying( block_id ) )
        {
        }

        uint16_t u : k_max_u_bit;
        uint16_t v : k_max_v_bit;
        uint16_t block_id : k_max_block_id_bit;
    };

    static_assert(
        sizeof( VertexTextureDescr ) == sizeof( uint32_t ),
        "TextureCoords should be 4 byte for correct work" );

    /*
     * Class that used to indexing by local coordinates in array of blocks
     * that should be rendered. Render area is a square with the side
     * equal to 2 * k_render_distance + 1 chunks.
     */

    struct __attribute__( ( packed ) ) RenderAreaBlockPos
    {
      private:
        constexpr static auto max_x_bits = 11;
        constexpr static auto max_y_bits = 11;
        constexpr static auto max_z_bits = 10;

      public:
        constexpr static auto k_max_x = ( 1 << max_x_bits ) - 1;
        constexpr static auto k_max_y = ( 1 << max_y_bits ) - 1;
        constexpr static auto k_max_z = ( 1 << max_z_bits ) - 1;

        constexpr RenderAreaBlockPos( uint16_t local_x, uint16_t local_y, uint16_t local_z )
            : x( local_x ),
              y( local_y ),
              z( local_z ){};

        // coordinate system starting right corner of
        // render area
        uint16_t x : max_x_bits;
        uint16_t y : max_y_bits;
        uint16_t z : max_z_bits;

        static_assert(
            k_max_x >= Chunk::k_max_width_length * ( 2 * ChunkMan::k_render_distance + 1 ),
            "Cannot use local coordinates because render distance is too large" );
    };

    static_assert(
        sizeof( RenderAreaBlockPos ) == sizeof( uint32_t ),
        "RenderAreaBlockPos should be 4 byte for correct work" );

    /*
     * struct of vertex that will be sent to vertex shader
     */
    struct __attribute__( ( packed ) ) Vertex
    {
        constexpr Vertex( RenderAreaBlockPos position_par, BlockID block_id, uint16_t width, uint16_t height )
            : position( position_par ),
              tex_descr( block_id, width, height )
        {
        }

        RenderAreaBlockPos position;
        VertexTextureDescr tex_descr;
    };

    static_assert( sizeof( Vertex ) == sizeof( uint64_t ), "Vertex should be 8 byte for correct work" );

    /*
     * Specify information needed for vulkan about vertex format
     */
    struct VertexInfo
    {
        std::array<vk::VertexInputBindingDescription, 1> binding_descr;
        std::array<vk::VertexInputAttributeDescription, 2> attribute_descr;
    };

    struct FaceInfo
    {
        bool is_front_face; /* Define the order of indices in index buffer.
                             * Watch addFace() realisition
                             */
        BlockID block_id;   /* block id that face are representing */
        int width;          /* width of the face */
        int height;         /* height of the face */

        RenderAreaBlockPos v1; /* first vertex of the face */
        RenderAreaBlockPos v2; /* second vertex of the face */
        RenderAreaBlockPos v3; /* third vertex of the face */
        RenderAreaBlockPos v4; /* fourth vertex of the face */
    };

  private:
    /*
     * add face of block. The face_info is const auto&, because you cannot define
     * this function from the outside, if you pick const FaceInfo& ( because it's private )
     */
    void addFace( const auto& face_info );

    /*
     * Convert 3 coordinates of type uint16_t to local coordinates and return RenderAreaBlockPos.
     * Here is auto as return value because RenderAreaBlockPos is private structure
     */

    auto toRenderAreaBlockPos( uint16_t x, uint16_t y, uint16_t z, const pos::ChunkPos& chunk_pos ) const;

  public:
    /*
     * Index type in index buffer
     */
    constexpr static vk::IndexType k_index_type = vk::IndexType::eUint32;

  public:
    /*
     * Mesh the area that player can see
     */
    void meshRenderArea();

    /*
     * Algorithm for meshing one chunk
     */
    void greedyMesh( const pos::ChunkPos& chunk_pos, const Chunk& chunk );

    /*
     * Get description of the vertex format that used for meshing
     */

    VertexInfo getVertexInfo() const;

    uint32_t getVerticesCount() const { return m_vertices.size(); }

    const Vertex* getVerticesData() const { return m_vertices.data(); };

    uint32_t getVertexBufferSize() const { return m_vertices.size() * sizeof( Vertex ); }

    uint32_t getIndicesCount() const { return m_indices.size(); }

    const uint32_t* getIndicesData() const { return m_indices.data(); };

    uint32_t getIndexBufferSize() const { return m_indices.size() * sizeof( uint32_t ); }

    /*
     * [krisszzz]: This function should be private and used only for
     * updating uniform buffer. I will do it soon
     */
    pos::ChunkPos getRenderAreaRight() const { return m_render_area_right; }

    uint32_t getAllocatedBytesCount() const
    {
        return m_vertices.capacity() * sizeof( Vertex ) + m_indices.capacity() * sizeof( uint32_t );
    }

  private:
    /* right corner of render area */
    pos::ChunkPos m_render_area_right;
    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_indices;
}; // class ChunkMesher
}; // namespace chunk
