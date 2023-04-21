#pragma once

#include "common/vulkan_include.h"

#include "utils/algorithm.h"

#include <range/v3/algorithm/any_of.hpp>
#include <range/v3/range/concepts.hpp>

namespace vkwrap
{

template <ranges::range Range>
inline void
writeSuitableSharingInfo( auto& create_info, Range&& indices )
{
    auto m_unique_indices = utils::getUniqueElements( indices );

    if ( m_unique_indices.size() >= 2 )
    {
        create_info.setSharingMode( vk::SharingMode::eConcurrent );
        create_info.setQueueFamilyIndexCount( static_cast<uint32_t>( m_unique_indices.size() ) );
        create_info.setPQueueFamilyIndices( m_unique_indices.data() );
    } else
    {
        create_info.setSharingMode( vk::SharingMode::eExclusive );
        create_info.setQueueFamilyIndexCount( 0 );
        create_info.setPQueueFamilyIndices( nullptr );
    }
} // writeSuitableSharingInfo

inline bool
isDepthOnly( vk::Format format )
{
    using vk::Format;

    constexpr auto k_depth_only_formats = std::array{ Format::eD16Unorm, Format::eD32Sfloat };

    return ranges::any_of( k_depth_only_formats, [ format ]( Format depth_only_format ) {
        return format == depth_only_format;
    } );
} // isDepthOnly

inline bool
isStencilOnly( vk::Format format )
{
    using vk::Format;

    constexpr auto k_stencil_only_formats = std::array{ Format::eS8Uint };

    return ranges::any_of( k_stencil_only_formats, [ format ]( Format stencil_only_format ) {
        return format == stencil_only_format;
    } );
} // isStencilOnly

inline bool
isDepthStencilOnly( vk::Format format )
{
    using vk::Format;

    constexpr auto k_combined_formats =
        std::array{ Format::eD16UnormS8Uint, Format::eD24UnormS8Uint, Format::eD32SfloatS8Uint };

    return ranges::any_of( k_combined_formats, [ format ]( Format combined_format ) {
        return format == combined_format;
    } );
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

} // namespace vkwrap
