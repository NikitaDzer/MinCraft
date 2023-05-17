#pragma once

#include "common/vulkan_include.h"

#include <range/v3/numeric/accumulate.hpp>
#include <range/v3/range/concepts.hpp>
#include <range/v3/range/conversion.hpp>

#include <concepts>

namespace vkwrap
{

class DescriptorPool : private vk::UniqueDescriptorPool
{
  private:
    using Base = vk::UniqueDescriptorPool;

  public:
    using Base::operator bool;
    using Base::operator->;
    using Base::operator*;
    using Base::get;

  private:
    template <ranges::range PoolSizes>
        requires std::same_as<ranges::range_value_t<PoolSizes>, vk::DescriptorPoolSize>
    static vk::UniqueDescriptorPool createDescriptorPool( vk::Device logical_device, PoolSizes pool_sizes )
    {
        uint32_t max_sets =
            ranges::accumulate( pool_sizes, uint32_t{ 0 }, {}, &vk::DescriptorPoolSize::descriptorCount );

        auto pool_sizes_vec = ranges::to_vector( pool_sizes );
        auto descriptor_info = vk::DescriptorPoolCreateInfo{
            .flags = vk::DescriptorPoolCreateFlagBits::eFreeDescriptorSet,
            .maxSets = max_sets,
            .poolSizeCount = static_cast<uint32_t>( pool_sizes_vec.size() ),
            .pPoolSizes = pool_sizes_vec.data() };

        return logical_device.createDescriptorPoolUnique( descriptor_info );
    }

  public:
    template <ranges::range PoolSizes>
        requires std::same_as<ranges::range_value_t<PoolSizes>, vk::DescriptorPoolSize>
    DescriptorPool( vk::Device logical_device, PoolSizes pool_sizes )
        : Base{ createDescriptorPool( logical_device, pool_sizes ) }
    {
    }

    DescriptorPool( vk::UniqueDescriptorPool descriptor_pool )
        : Base{ std::move( descriptor_pool ) }
    {
    }

    operator vk::DescriptorPool() const { return get(); }
};

} // namespace vkwrap