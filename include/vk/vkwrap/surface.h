#pragma once

#include "common/glfw_include.h"
#include "common/vulkan_include.h"

namespace vkwrap
{

class Surface : private vk::UniqueSurfaceKHR
{

  private:
    using BaseType = vk::UniqueSurfaceKHR;

  public:
    Surface( vk::SurfaceKHR surface )
        : BaseType{ surface }
    {
    }

    bool isSupportedBy( vk::PhysicalDevice physical_device );

    using BaseType::get;
    using BaseType::operator*;
    using BaseType::operator->;
    using BaseType::operator bool;

}; // class Surface

} // namespace vkwrap
