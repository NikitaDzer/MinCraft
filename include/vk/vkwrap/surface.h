#pragma once

#include "common/glfw_include.h"
#include "common/vulkan_include.h"

#include "window/window.h"

namespace vkwrap
{

class Surface : private vk::UniqueSurfaceKHR
{

  private:
    using BaseType = vk::UniqueSurfaceKHR;

  public:
    // Call permissions: any thread.
    Surface( vk::Instance instance, wnd::Window& window )
        : BaseType{ wnd::Window::createWindowSurface( instance, window ) }
    {
    }

    bool isSupportedBy( vk::PhysicalDevice physical_device );

    using BaseType::get;
    using BaseType::operator*;
    using BaseType::operator->;
    using BaseType::operator bool;

}; // class Surface

} // namespace vkwrap
