#pragma once

#include "common/vulkan_include.h"

#include "vkwrap/core.h"
#include "vkwrap/mman.h"
#include "vkwrap/queues.h"
#include "vkwrap/utils.h"
#include "vkwrap/image_view.h"

#include "utils/patchable.h"

#include <range/v3/range/concepts.hpp>
#include <range/v3/range/conversion.hpp>

#include <concepts>
#include <exception>
#include <functional>
#include <optional>
#include <utility>

#include <cassert>

namespace vkwrap
{

class Image : private vk::Image
{

  private:
    using Base = vk::Image;

  private:
    Mman* m_mman{ nullptr };
    ImageView m_view;

    ImageView createView( const vk::ImageCreateInfo& create_info ) const
    {
        ImageViewBuilder builder{};

        builder.withImage( *this )
            .withImageType( create_info.imageType )
            .withFormat( create_info.format )
            .withComponents( k_components )
            .withLayerCount( create_info.arrayLayers );

        return builder.make( m_mman->getDevice() );
    } // createView

    /**
     * Don't remap color components.
     */
    static constexpr vk::ComponentMapping k_components{};

    static vk::Image createImage( const vk::ImageCreateInfo& create_info, Mman& mman )
    {
        return mman.create( create_info );
    } // createImage

    friend void swap( Image& lhs, Image& rhs )
    {
        std::swap( lhs.get(), rhs.get() );
        std::swap( lhs.m_mman, rhs.m_mman );
        std::swap( lhs.m_view, rhs.m_view );
    } // swap

  public:
    Image( const vk::ImageCreateInfo& create_info, Mman& mman )
        : Base{ createImage( create_info, mman ) },
          m_mman{ &mman },
          m_view{ createView( create_info ) }
    {
    } // Image

    ~Image()
    {
        /** There cannot be an exception.
         *
         * 1. If `createImage` throws an exception in constructor,
         * then `m_mman == nullptr` and we won't `destroy` an object.
         *
         * 2. If constructor completes successfully,
         * then `Mman::findInfo` CANNOT throw an exception:
         * - info must be found in `Mman::m_images_info`
         * - as `vk::Image` and `Mman::ImageInfo` have only noexcept methods,
         *   then `unordered_map::erase` cannot throw an exception
         */
        if ( m_mman != nullptr )
        {
            m_mman->destroy( *this );
        }
    } // ~Image

    Image( Image&& image )
    {
        swap( *this, image );
    } // Image( Image&& )

    Image& operator=( Image&& image )
    {
        Image tmp{ std::move( image ) };
        swap( *this, tmp );

        return *this;
    } // operator=( Image&& )

    Image( const Image& ) = delete;
    Image& operator=( const Image& ) = delete;

    void update( vk::Buffer src_buffer ) { m_mman->copy( src_buffer, *this ); }
    void update( vk::Buffer src_buffer, Mman::RegionMaker maker )
    {
        m_mman->copy( src_buffer, *this, maker );
    } // update

    void transit( vk::ImageLayout new_layout ) { m_mman->transit( *this, new_layout ); }

    vk::ImageView getView() const { return m_view.get(); }

    Base& get() { return static_cast<Base&>( *this ); }
    const Base& get() const { return static_cast<const Base&>( *this ); }

    using Base::operator bool;
    using Base::operator VkImage;

}; // class Image

class ImageBuilder
{

  private:
    using Queues = std::vector<Queue>;

  public:
    // clang-format off
    PATCHABLE_DEFINE_STRUCT( 
        ImagePartialInfo,
        ( std::optional<vk::ImageType>,           image_type   ),
        ( std::optional<vk::Format>,              format       ),
        ( std::optional<vk::Extent3D>,            extent       ),
        ( std::optional<uint32_t>,                array_layers ),
        ( std::optional<vk::SampleCountFlagBits>, samples      ),
        ( std::optional<vk::ImageTiling>,         tiling       ),
        ( std::optional<vk::ImageUsageFlags>,     usage        ),
        ( std::optional<Queues>,                  queues       )
    );
    // clang-format on

    using Setter = std::function<void( ImagePartialInfo& )>;

  private:
    // clang-format off
    Setter m_setter = []( auto& ){};
    // clang-format on

    ImagePartialInfo m_partial;

    ImagePartialInfo makePartialInfo() const
    {
        ImagePartialInfo partial{};

        m_presetter( partial );
        m_setter( partial );
        partial.patchWith( m_partial );

        return partial;
    } // makePartialInfo

    // clang-format off
    static inline Setter m_presetter = []( auto& ){};

    static constexpr vk::ImageCreateInfo k_initial_create_info{
        // We don't use these specific fields.
        .pNext = {},
        .flags = {},

        // We don't use compressed images.
        .mipLevels = 1,

        // Initial layout is unknown.
        .initialLayout = vk::ImageLayout::eUndefined
    };
    // clang-format on

  public:
    ImageBuilder() = default;

    ImageBuilder& withSetter( Setter setter ) &
    {
        assert( setter );

        m_setter = setter;
        return *this;
    } // withSetter

    ImageBuilder& withImageType( vk::ImageType type ) &
    {
        m_partial.image_type = type;
        return *this;
    } // withImageType

    ImageBuilder& withFormat( vk::Format format ) &
    {
        m_partial.format = format;
        return *this;
    } // withFormat

    ImageBuilder& withExtent( vk::Extent3D extent ) &
    {
        m_partial.extent = extent;
        return *this;
    } // withExtent

    ImageBuilder& withArrayLayers( uint32_t array_layers )
    {
        assert( array_layers >= 1 );

        m_partial.array_layers = array_layers;
        return *this;
    } // withArrayLayers

    ImageBuilder& withSampleCount( vk::SampleCountFlagBits samples ) &
    {
        m_partial.samples = samples;
        return *this;
    } // withSampleCount

    ImageBuilder& withTiling( vk::ImageTiling tiling ) &
    {
        m_partial.tiling = tiling;
        return *this;
    } // withTiling

    ImageBuilder& withUsage( vk::ImageUsageFlags usage ) &
    {
        m_partial.usage = usage;
        return *this;
    } // withUsage

    template <ranges::range Range>
        requires std::same_as<ranges::range_value_t<Range>, Queue>
    ImageBuilder& withQueues( Range&& queues ) &
    {
        assert( !queues.empty() );

        m_partial.queues = ranges::to_vector( queues );
        return *this;
    } // withQueues

    Image make( Mman& mman ) const&
    {
        ImagePartialInfo partial{ makePartialInfo() };
        partial.assertCheckMembers();
        assert( !partial.queues->empty() );

        vk::ImageCreateInfo create_info{ k_initial_create_info };
        create_info.imageType = *partial.image_type;
        create_info.format = *partial.format;
        create_info.extent = *partial.extent;
        create_info.arrayLayers = *partial.array_layers;
        create_info.samples = *partial.samples;
        create_info.tiling = *partial.tiling;
        create_info.usage = *partial.usage;

        SharingInfoSetter setter{ *partial.queues };
        setter.setTo( create_info );

        return { create_info, mman };
    } // make

    static void setPresetter( Setter presetter )
    {
        assert( presetter );
        m_presetter = presetter;
    } // setPresetter

}; // class ImageBuilder

} // namespace vkwrap
