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

inline vk::Extent2D
getSurfaceExtent( vk::PhysicalDevice physical_device, 
                  vk::SurfaceKHR surface )
{
    return physical_device.getSurfaceCapabilitiesKHR( surface ).currentExtent;
} // getSurfaceExtent

inline vk::Extent2D
getSurfaceMinExtent( vk::PhysicalDevice physical_device, 
                     vk::SurfaceKHR surface )
{
    return physical_device.getSurfaceCapabilitiesKHR( surface ).minImageExtent;
} // getSurfaceExtent

inline vk::Extent2D
getSurfaceMaxExtent( vk::PhysicalDevice physical_device, 
                     vk::SurfaceKHR surface )
{
    return physical_device.getSurfaceCapabilitiesKHR( surface ).maxImageExtent;
} // getSurfaceExtent

inline vk::SurfaceTransformFlagBitsKHR
getSurfaceCurrentTransform( vk::PhysicalDevice physical_device,
                            vk::SurfaceKHR surface )
{
    return physical_device.getSurfaceCapabilitiesKHR( surface ).currentTransform;
} // getSurfaceCurrentTransform

} // namespace vkwrap
