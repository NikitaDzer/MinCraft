#pragma once

#include "common/vulkan_include.h"

namespace vkwrap
{

class Surface : private vk::UniqueSurfaceKHR
{

  private:
    using BaseType = vk::UniqueSurfaceKHR;

  public:
    Surface( vk::UniqueSurfaceKHR surface )
        : BaseType{ std::move( surface ) }
    {
    }

    bool isSupportedBy( vk::PhysicalDevice physical_device ) const;

    using BaseType::get;
    using BaseType::operator*;
    using BaseType::operator->;
    using BaseType::operator bool;

}; // class Surface

bool physicalDeviceSupportsPresent( vk::PhysicalDevice, vk::SurfaceKHR );

} // namespace vkwrap
