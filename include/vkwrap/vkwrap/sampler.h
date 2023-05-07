#pragma once

#include "common/vulkan_include.h"

#include "vkwrap/core.h"
#include "vkwrap/mman.h"
#include "vkwrap/utils.h"

#include "utils/patchable.h"

#include <range/v3/range/concepts.hpp>
#include <range/v3/range/conversion.hpp>

#include <functional>
#include <optional>
#include <utility>

namespace vkwrap
{

class Sampler : private vk::UniqueSampler
{

  private:
    using Base = vk::UniqueSampler;

  public:
    Sampler( vk::UniqueSampler unique_sampler )
        : Base{ std::move( unique_sampler ) }
    {
    }

    using Base::operator bool;
    using Base::operator->;
    using Base::get;

}; // class Sampler

class SamplerBuilder
{

  public:
    // clang-format off
    PATCHABLE_DEFINE_STRUCT( 
        SamplerPartialInfo,
        ( std::optional<vk::Filter>,             mag_filter               ),
        ( std::optional<vk::Filter>,             min_filter               ),
        ( std::optional<vk::SamplerAddressMode>, address_mode_u           ),
        ( std::optional<vk::SamplerAddressMode>, address_mode_v           ),
        ( std::optional<vk::SamplerAddressMode>, address_mode_w           ),
        ( std::optional<vk::Bool32>,             anisotropy_enable        ),
        ( std::optional<vk::CompareOp>,          compare_op               ),
        ( std::optional<vk::BorderColor>,        border_color             ),
        ( std::optional<vk::Bool32>,             unnormalized_coordinates )
    );
    // clang-format on

    using Setter = std::function<void( SamplerPartialInfo& )>;

  private:
    // clang-format off
    Setter m_setter = []( auto& ){};
    // clang-format on

    SamplerPartialInfo m_partial;

    SamplerPartialInfo makePartialInfo() const
    {
        SamplerPartialInfo partial{};

        m_presetter( partial );
        m_setter( partial );
        partial.patchWith( m_partial );

        return partial;
    } // makePartialInfo

    static float getMaxAnisotropy( vk::PhysicalDevice physical_device )
    {
        return physical_device.getProperties().limits.maxSamplerAnisotropy;
    } // getMaxAnisotropy

    // clang-format off
    static inline Setter m_presetter = []( auto& ){};

    static constexpr vk::SamplerCreateInfo k_initial_create_info{
        // We don't use these specific fields.
        .pNext = {},
        .flags = {},

        // We use images with only one mipmap.
        .mipmapMode = vk::SamplerMipmapMode::eLinear,
        .mipLodBias = 0.0f,

        // compareOp would define behaviour of comparison.
        .compareEnable = VK_TRUE,

        // We use images with only one mipmap.
        .minLod = 0.0f,
        .maxLod = 0.0f,
    };
    // clang-format on

  public:
    SamplerBuilder() = default;

    SamplerBuilder& withSetter( Setter setter ) &
    {
        assert( setter );

        m_setter = setter;
        return *this;
    } // withSetter

    SamplerBuilder& withMagFilter( vk::Filter filter ) &
    {
        m_partial.mag_filter = filter;
        return *this;
    } // withMagFilter

    SamplerBuilder& withMinFilter( vk::Filter filter ) &
    {
        m_partial.min_filter = filter;
        return *this;
    } // withMinFilter

    SamplerBuilder& withAddressModeU( vk::SamplerAddressMode mode ) &
    {
        m_partial.address_mode_u = mode;
        return *this;
    } // withAddressModeU

    SamplerBuilder& withAddressModeV( vk::SamplerAddressMode mode ) &
    {
        m_partial.address_mode_v = mode;
        return *this;
    } // withAddressModeV

    SamplerBuilder& withAddressModeW( vk::SamplerAddressMode mode ) &
    {
        m_partial.address_mode_w = mode;
        return *this;
    } // withAddressModeW

    SamplerBuilder& withAnisotropyEnable( vk::Bool32 is_enable ) &
    {
        m_partial.anisotropy_enable = is_enable;
        return *this;
    } // withAnisotropyEnable

    SamplerBuilder& withCompareOp( vk::CompareOp cmp_op ) &
    {
        m_partial.compare_op = cmp_op;
        return *this;
    } // withCompareOp

    SamplerBuilder& withBorderColor( vk::BorderColor color ) &
    {
        m_partial.border_color = color;
        return *this;
    } // withBorderColor

    SamplerBuilder& withUnnormalizedCoordinates( vk::Bool32 is_unnormalized ) &
    {
        m_partial.unnormalized_coordinates = is_unnormalized;
        return *this;
    } // withUnnormalizedCoordinates

    Sampler make( vk::Device device, vk::PhysicalDevice physical_device ) const&
    {
        SamplerPartialInfo partial{ makePartialInfo() };
        partial.assertCheckMembers();

        vk::SamplerCreateInfo create_info{ k_initial_create_info };

        create_info.magFilter = *partial.mag_filter;
        create_info.minFilter = *partial.min_filter;

        create_info.addressModeU = *partial.address_mode_u;
        create_info.addressModeV = *partial.address_mode_v;
        create_info.addressModeW = *partial.address_mode_w;

        create_info.anisotropyEnable = *partial.anisotropy_enable;
        create_info.maxAnisotropy = getMaxAnisotropy( physical_device );

        create_info.compareOp = *partial.compare_op;
        create_info.unnormalizedCoordinates = *partial.unnormalized_coordinates;

        return { device.createSamplerUnique( create_info ) };
    } // make

    static void setPresetter( Setter presetter )
    {
        assert( presetter );
        m_presetter = presetter;
    } // setPresetter

}; // class SamplerBuilder

} // namespace vkwrap
