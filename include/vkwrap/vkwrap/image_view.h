#pragma once

#include "common/vulkan_include.h"

#include "utils/patchable.h"

#include "vkwrap/core.h"
#include "vkwrap/utils.h"

#include <optional>

#include <cassert>

namespace vkwrap
{

class ImageView : private vk::UniqueImageView
{

  private:
    using Base = vk::UniqueImageView;

  public:
    ImageView() = default;

    ImageView( vk::UniqueImageView image_view )
        : Base{ std::move( image_view ) }
    {
    } // ImageView

    using Base::operator bool;
    using Base::operator->;
    using Base::get;

}; // class ImageView

class ImageViewBuilder
{

  public:
    // clang-format off
    PATCHABLE_DEFINE_STRUCT(
        ImageViewPartialInfo,
        ( std::optional<vk::Image>,            image       ),
        ( std::optional<vk::ImageType>,        image_type  ),
        ( std::optional<vk::Format>,           format      ),
        ( std::optional<vk::ComponentMapping>, components  ),
        ( std::optional<uint32_t>,             layer_count )
    );
    // clang-format on

    using Setter = std::function<void( ImageViewPartialInfo& )>;

  private:
    // clang-format off
    Setter m_setter = []( auto& ){};
    // clang-format on

    ImageViewPartialInfo m_partial;

    ImageViewPartialInfo makePartialInfo() const
    {
        ImageViewPartialInfo partial{};

        m_presetter( partial );
        m_setter( partial );
        partial.patchWith( m_partial );

        return partial;
    } // makePartialInfo

    // clang-format off
    static inline Setter m_presetter = []( auto& ){};

    static constexpr vk::ImageViewCreateInfo k_initial_create_info{
        // We don't use that specific fields.
        .pNext = {},
        .flags = {},

        .subresourceRange = { 
            // We use images with only one mipmap.
            .baseMipLevel = 0, 
            .levelCount = 1,

            .baseArrayLayer = 0 }
    };
    // clang-format on

  public:
    ImageViewBuilder() = default;

    ImageViewBuilder& withSetter( Setter setter ) &
    {
        assert( setter );

        m_setter = setter;
        return *this;
    } // withSetter

    ImageViewBuilder& withImage( vk::Image image ) &
    {
        m_partial.image = image;
        return *this;
    } // withImage

    ImageViewBuilder& withImageType( vk::ImageType image_type ) &
    {
        m_partial.image_type = image_type;
        return *this;
    } // withImageType

    ImageViewBuilder& withFormat( vk::Format format ) &
    {
        m_partial.format = format;
        return *this;
    } // withFormat

    ImageViewBuilder& withComponents( vk::ComponentMapping components ) &
    {
        m_partial.components = components;
        return *this;
    } // wtihComponents

    ImageViewBuilder& withLayerCount( uint32_t layer_count ) &
    {
        m_partial.layer_count = layer_count;
        return *this;
    } // withLayerCount

    ImageView make( vk::Device device ) const&
    {
        ImageViewPartialInfo partial{ makePartialInfo() };
        partial.assertCheckMembers();

        vk::ImageViewCreateInfo create_info{ k_initial_create_info };

        create_info.image = *partial.image;
        create_info.viewType = chooseImageViewType( *partial.image_type );
        create_info.format = *partial.format;

        create_info.subresourceRange.aspectMask = chooseAspectMask( *partial.format );
        create_info.subresourceRange.layerCount = *partial.layer_count;

        return device.createImageViewUnique( create_info );
    } // make

    static void setPresetter( Setter presetter )
    {
        assert( presetter );
        m_presetter = presetter;
    } // setPresetter

}; // class ImageViewBuilder

} // namespace vkwrap
