#pragma once

#include "vkwrap/core.h"
#include "vkwrap/error.h"

#include "common/utils.h"
#include "common/vulkan_include.h"

#include <range/v3/range.hpp>
#include <range/v3/view/transform.hpp>

#include <string_view>
#include <vector>

namespace vkwrap
{

class PhysicalDevice : private vk::PhysicalDevice
{
  public:
    PhysicalDevice() = default;

    PhysicalDevice( const vk::PhysicalDevice& pdev )
        : vk::PhysicalDevice{ pdev }
    {
    }

    template <ranges::range Range> [[nodiscard]] SupportsResult supportsExtensions( Range&& extensions )
    {
        const auto supported_extensions = vk::PhysicalDevice::enumerateDeviceExtensionProperties();
        auto missing = utils::findAllMissing<std::string>( supported_extensions, extensions, []( auto&& a ) {
            return std::string_view{ a.extensionName };
        } );
        return { missing.empty(), missing };
    }

    vk::PhysicalDevice base() const { return static_cast<vk::PhysicalDevice>( *this ); }

    using vk::PhysicalDevice::createDevice;
    using vk::PhysicalDevice::createDeviceUnique;
    using vk::PhysicalDevice::enumerateDeviceExtensionProperties;
    using vk::PhysicalDevice::enumerateDeviceLayerProperties;

    using vk::PhysicalDevice::getMemoryProperties;
    using vk::PhysicalDevice::getProperties;

    using vk::PhysicalDevice::getQueueFamilyProperties;
    using vk::PhysicalDevice::getSurfaceCapabilitiesKHR;
    using vk::PhysicalDevice::getSurfacePresentModesKHR;
    using vk::PhysicalDevice::getSurfaceSupportKHR;

    using vk::PhysicalDevice::operator bool;
};

template <ranges::range ExtType = ranges::empty_view<const char*>>
std::vector<PhysicalDevice>
enumerateSuitablePhysicalDevices( const vk::Instance& instance, VulkanVersion version, ExtType&& extensions )
{
    const auto pdevs_raw = instance.enumeratePhysicalDevices();
    auto pdevs_proj = ranges::views::transform(
        pdevs_raw, // clang-format off
        []( auto&& elem ) { return PhysicalDevice{ elem }; } 
    ); // clang-format on

    const auto fits_requirements = [ &version, &extensions ]( auto&& pdev ) -> bool {
        const auto props = pdev.getProperties();

        const auto version_fits = ( props.apiVersion >= utils::toUnderlying( version ) );
        const auto extensions_supported = pdev.supportsExtensions( extensions ).first;

        return version_fits && extensions_supported;
    };

    return pdevs_proj | ranges::views::remove_if( [ & ]( auto&& pdev ) { return !fits_requirements( pdev ); } ) |
        ranges::to_vector;
}

} // namespace vkwrap