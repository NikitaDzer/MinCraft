#pragma once

#include "common/vulkan_include.h"
#include "vkwrap/core.h"

#include <range/v3/view/all.hpp>

#include <vector>

namespace imgw
{

namespace detail
{

class InstanceInfo
{
  public:
    InstanceInfo( vk::Instance instance )
        : m_version{ static_cast<vkwrap::VulkanVersion>( vk::enumerateInstanceVersion() ) },
          m_extensions{ vk::enumerateInstanceExtensionProperties() },
          m_layers{ vk::enumerateInstanceLayerProperties() }
    {
    }

    auto version() const { return m_version; }
    auto extensions() const { return ranges::views::all( m_extensions ); }
    auto layers() const { return ranges::views::all( m_layers ); }

  private:
    vkwrap::VulkanVersion m_version;
    std::vector<vk::ExtensionProperties> m_extensions;
    std::vector<vk::LayerProperties> m_layers;
};

class SurfaceInfo
{
  public:
    SurfaceInfo( vk::PhysicalDevice physical_device, vk::SurfaceKHR surface )
        : m_supported_formats{ physical_device.getSurfaceFormatsKHR( surface ) },
          m_present_modes{ physical_device.getSurfacePresentModesKHR( surface ) },
          m_capabilites{ physical_device.getSurfaceCapabilitiesKHR( surface ) }
    {
    }

    auto format() const { return ranges::views::all( m_supported_formats ); }
    auto modes() const { return ranges::views::all( m_present_modes ); }
    auto& capabilities() const& { return m_capabilites; }

  private:
    std::vector<vk::SurfaceFormatKHR> m_supported_formats;
    std::vector<vk::PresentModeKHR> m_present_modes;
    vk::SurfaceCapabilitiesKHR m_capabilites;
};

class PhysicalDeviceInfo
{
  public:
    PhysicalDeviceInfo( vk::PhysicalDevice device, vk::SurfaceKHR surface )
        : m_properties{ device.getProperties() },
          m_features{ device.getFeatures() },
          m_extensions{ device.enumerateDeviceExtensionProperties() },
          m_device{ device },
          m_surface{ surface }
    {
    }

    static std::vector<PhysicalDeviceInfo> allFromInstance( vk::Instance, vk::SurfaceKHR surface );

    auto& properties() const& { return m_properties; }
    auto features() const { return m_features; }
    auto extensions() const { return ranges::views::all( m_extensions ); }
    auto surface() const { return m_surface; }
    auto device() const { return m_device; }

  private:
    vk::PhysicalDeviceProperties m_properties;
    vk::PhysicalDeviceFeatures m_features;
    std::vector<vk::ExtensionProperties> m_extensions;
    vk::PhysicalDevice m_device;
    vk::SurfaceKHR m_surface;
};

using PhysicalDevicesInfo = std::vector<PhysicalDeviceInfo>;

class VulkanInformation
{
  public:
    VulkanInformation( vk::Instance instance, vk::SurfaceKHR surface )
        : instance{ instance },
          physical_devices{ PhysicalDeviceInfo::allFromInstance( instance, surface ) }
    {
    }

  public:
    InstanceInfo instance;
    PhysicalDevicesInfo physical_devices;
};

} // namespace detail

class VulkanInformationTab
{
  public:
    VulkanInformationTab( vk::Instance instance, vk::SurfaceKHR surface )
        : m_information{ instance, surface }
    {
    }

    void draw();

  private:
    detail::VulkanInformation m_information;
};

}; // namespace imgw