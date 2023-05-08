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

class PhysicalDeviceInfo
{
  public:
    PhysicalDeviceInfo( vk::PhysicalDevice device )
        : m_properties{ device.getProperties() },
          m_features{ device.getFeatures() },
          m_extensions{ device.enumerateDeviceExtensionProperties() }
    {
    }

    static std::vector<PhysicalDeviceInfo> allFromInstance( vk::Instance );

    auto properties() const { return m_properties; }
    auto features() const { return m_features; }
    auto extensions() const { return ranges::views::all( m_extensions ); }

  private:
    vk::PhysicalDeviceProperties m_properties;
    vk::PhysicalDeviceFeatures m_features;
    std::vector<vk::ExtensionProperties> m_extensions;
};

using PhysicalDevicesInfo = std::vector<PhysicalDeviceInfo>;

class VulkanInformation
{
  public:
    VulkanInformation( vk::Instance instance )
        : instance{ instance },
          physical_devices{ PhysicalDeviceInfo::allFromInstance( instance ) }
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
    VulkanInformationTab( vk::Instance instance )
        : m_information{ instance }
    {
    }

    void draw();

  private:
    detail::VulkanInformation m_information;
};

}; // namespace imgw