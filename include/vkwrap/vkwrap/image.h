#pragma once

#include "common/vulkan_include.h"

#include "vkwrap/core.h"
#include "vkwrap/mman.h"
#include "vkwrap/queues.h"
#include "vkwrap/utils.h"

#include "utils/patchable.h"

#include <range/v3/range/concepts.hpp>
#include <range/v3/range/conversion.hpp>

#include <exception>
#include <functional>
#include <optional>
#include <utility>

#include <cassert>

namespace vkwrap
{

namespace detail
{

inline vk::ImageViewType
chooseImageViewType( vk::ImageType type )
{
    using vk::ImageType;
    using vk::ImageViewType;

    switch ( type )
    {
    case ImageType::e1D:
        return ImageViewType::e1DArray;

    case ImageType::e2D:
        return ImageViewType::e2DArray;

    case ImageType::e3D:
        return ImageViewType::e3D;

    default:
        // Assert addresses to possible additions in VkImageType.
        assert( 0 && "chooseImageViewType: image type is not supported." );
        std::terminate();
    }
} // choosesImageViewType

} // namespace detail

class Image : private vk::Image
{

  private:
    using Base = vk::Image;

  private:
    Mman* m_mman{ nullptr };
    vk::UniqueImageView m_view;

    // clang-format off
    static constexpr vk::ImageViewCreateInfo k_view_initial_create_info{
        // We don't use these specific fields.
        .pNext = {},
        .flags = {},

        // Don't remap color components.
        .components = {},

        // We use images with only one mipmap and one layer.
        .subresourceRange = { 
            .baseMipLevel = 0, 
            .levelCount = 1, 
            .baseArrayLayer = 0 }
    };
    // clang-format on

    static vk::Image createImage( const vk::ImageCreateInfo& create_info, Mman& mman )
    {
        return mman.create( create_info );
    } // createImage

    static vk::ImageViewCreateInfo makeImageViewCreateInfo(
        const vk::ImageCreateInfo& image_create_info,
        vk::Image image )
    {
        vk::ImageViewCreateInfo view_create_info{ k_view_initial_create_info };

        view_create_info.setImage( image );
        view_create_info.setViewType( detail::chooseImageViewType( image_create_info.imageType ) );
        view_create_info.setFormat( image_create_info.format );

        view_create_info.subresourceRange.setAspectMask( chooseAspectMask( image_create_info.format ) );
        view_create_info.subresourceRange.setLayerCount( image_create_info.arrayLayers );

        return view_create_info;
    } // makeImageViewCreateInfo

    static vk::UniqueImageView createImageView( const vk::ImageViewCreateInfo& create_info, vk::Device device )
    {
        return device.createImageViewUnique( create_info );
    } // createImageView

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
          m_view{ createImageView( makeImageViewCreateInfo( create_info, *this ), mman.getDevice() ) } //
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
        : Base{ nullptr },
          m_mman{ nullptr }
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

  public:
    using Queues = std::vector<Queue>;

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

        presetter( partial );
        m_setter( partial );
        partial.patchWith( m_partial );

        return partial;
    } // makePartialInfo

    static constexpr vk::ImageCreateInfo k_initial_create_info = {
        // We don't use these specific fields.
        .pNext = {},
        .flags = {},

        // We don't use compressed images.
        .mipLevels = 1,

        // Initial layout is unknown.
        .initialLayout = vk::ImageLayout::eUndefined };

  public:
    // clang-format off
    static inline Setter presetter = []( auto& ){};
    // clang-format on

    ImageBuilder() = default;

    ImageBuilder& withSetter( Setter setter ) &
    {
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

    ImageBuilder& withQueues( ranges::range auto&& queues ) &
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

        SharingInfoSetter setter{ partial.queues.value() };
        setter.setTo( create_info );

        return { create_info, mman };
    } // make

}; // class ImageBuilder

} // namespace vkwrap
