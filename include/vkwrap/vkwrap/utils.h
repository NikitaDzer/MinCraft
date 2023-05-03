#pragma once

#include "common/vulkan_include.h"

#include "vkwrap/core.h"

#include "utils/algorithm.h"

#include <range/v3/algorithm/contains.hpp>
#include <range/v3/range/concepts.hpp>
#include <range/v3/view.hpp>

namespace vkwrap
{

class SharingInfoSetter
{

  private:
    std::vector<QueueFamilyIndex> m_unique_indices;
    vk::SharingMode m_mode;

  public:
    SharingInfoSetter( ranges::range auto&& queues )
    {
        // clang-format off
        auto indices = ranges::views::transform( queues, []( auto&& queue ) { 
            return queue.familyIndex(); 
        });
        // clang-format on

        m_unique_indices = utils::getUniqueElements( indices );

        if ( m_unique_indices.size() >= 2 )
        {
            m_mode = vk::SharingMode::eConcurrent;
        } else
        {
            m_mode = vk::SharingMode::eExclusive;
        }
    } // SharingInfoSetter

    void setTo( auto& create_info ) const&
    {
        using InfoType = decltype( create_info );

        constexpr bool has_setSharingMode = requires ( InfoType t ) { t.setSharingMode( m_mode ); };

        constexpr bool has_setImageSharingMode = requires ( InfoType t ) { t.setImageSharingMode( m_mode ); };

        if constexpr ( has_setSharingMode )
        {
            create_info.setSharingMode( m_mode );
        } else if constexpr ( has_setImageSharingMode )
        {
            create_info.setImageSharingMode( m_mode );
        } else
        {
            static_assert(
                has_setSharingMode || has_setImageSharingMode,
                "setTo: Unimplemented vk::SharingInfo setter." );
        }

        create_info.setQueueFamilyIndexCount( static_cast<uint32_t>( m_unique_indices.size() ) );
        create_info.setPQueueFamilyIndices( m_unique_indices.data() );
    } // setTo

}; // class SharingInfoSetter

inline bool
isDepthOnly( vk::Format format )
{
    using vk::Format;

    constexpr auto k_depth_only_formats = std::array{ Format::eD16Unorm, Format::eD32Sfloat };

    return ranges::contains( k_depth_only_formats, format );
} // isDepthOnly

inline bool
isStencilOnly( vk::Format format )
{
    using vk::Format;

    constexpr auto k_stencil_only_formats = std::array{ Format::eS8Uint };

    return ranges::contains( k_stencil_only_formats, format );
} // isStencilOnly

inline bool
isDepthStencilOnly( vk::Format format )
{
    using vk::Format;

    constexpr auto k_combined_formats =
        std::array{ Format::eD16UnormS8Uint, Format::eD24UnormS8Uint, Format::eD32SfloatS8Uint };

    return ranges::contains( k_combined_formats, format );
} // isDepthStencilOnly

/** Acceptable formats:
 *  - contains Depth or Stencil
 *  - contains only Color
 *
 *  In other cases behaviour is undefined.
 */
inline vk::ImageAspectFlags
chooseAspectMask( vk::Format format )
{
    using vk::ImageAspectFlagBits;

    if ( isDepthOnly( format ) )
    {
        return ImageAspectFlagBits::eDepth;
    }

    if ( isStencilOnly( format ) )
    {
        return ImageAspectFlagBits::eStencil;
    }

    if ( isDepthStencilOnly( format ) )
    {
        return ImageAspectFlagBits::eDepth | ImageAspectFlagBits::eStencil;
    }

    return ImageAspectFlagBits::eColor;
} // chooseAspectMask

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
} // chooseImageViewType

} // namespace vkwrap
