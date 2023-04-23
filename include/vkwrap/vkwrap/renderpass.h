#pragma once

#include "common/vulkan_include.h"

namespace vkwrap
{

class RenderPass : vk::UniqueRenderPass
{
  private:
    using Base = vk::UniqueRenderPass;

  public:
    using Base::operator bool;
    using Base::operator->;
    using Base::operator*;
    using Base::get;

  public:
    RenderPass( vk::UniqueRenderPass descriptor_pool )
        : Base{ std::move( descriptor_pool ) }
    {
    }

    operator vk::DescriptorPool() const { return get(); }
};

} // namespace vkwrap