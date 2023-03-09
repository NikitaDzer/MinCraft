#include <cassert>

#include "common/glfw_include.h"
#include "common/vulkan_include.h"

#include "vkwrap/surface.h"

using namespace vkwrap;

vk::SurfaceKHR
Surface::createSurface( vk::Instance instance, GLFWwindow* window )
{
    assert( instance != nullptr );
    assert( window != nullptr );

    vk::SurfaceKHR surface{};
    glfwCreateWindowSurface(
        static_cast<VkInstance>( instance ),
        window,
        nullptr,
        reinterpret_cast<VkSurfaceKHR*>( &surface ) //
    );

    return surface;
} // createSurface

bool
Surface::isSupportedBy( vk::PhysicalDevice physical_device )
{
    assert( physical_device != nullptr );

    uint32_t queue_family_count = 0;
    physical_device.getQueueFamilyProperties(
        &queue_family_count,
        nullptr //
    );

    // clang-format off
    for ( uint32_t queue_family_index = 0; 
          queue_family_index < queue_family_count; 
          queue_family_index++ )
    {
        if ( physical_device.getSurfaceSupportKHR( queue_family_index, get() ) )
        {
            return true;
        }
    }
    // clang-format on

    return false;
} // isSupportedBy
