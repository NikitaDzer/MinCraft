#pragma once

#include "common/utils.h"
#include "common/vulkan_include.h"

#include "vkwrap/core.h"
#include "vkwrap/surface.h"

#include "range/v3/range/conversion.hpp"
#include <range/v3/algorithm/copy.hpp>
#include <range/v3/algorithm/sort.hpp>
#include <range/v3/algorithm/unique.hpp>
#include <range/v3/iterator/insert_iterators.hpp>
#include <range/v3/range/concepts.hpp>
#include <range/v3/view/all.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/iota.hpp>
#include <range/v3/view/zip.hpp>
#include <range/v3/view/zip_with.hpp>

#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace vkwrap
{

template <typename Range>
std::pair<bool, std::vector<std::string>>
physicalDeviceSupportsExtensions( vk::PhysicalDevice physical_device, Range&& extensions )
{
    auto supports = physical_device.enumerateDeviceExtensionProperties();
    auto missing = utils::findAllMissing( supports, extensions, &vk::ExtensionProperties::extensionName );
    return std::pair{ missing.empty(), missing };
}

class PhysicalDeviceSelector
{
  private:
    std::vector<std::string> m_extensions;                   // Extensions that the device needs to support
    std::unordered_map<vk::PhysicalDeviceType, int> m_types; // A range of device types to prefer
    vk::SurfaceKHR m_surface;                                // Surface to present to
    VulkanVersion m_version = VulkanVersion::e_version_1_0;  // Version the physical device has to support

  public:
    template <ranges::range Range> PhysicalDeviceSelector& withExtensions( Range&& extensions ) &
    {
        ranges::copy( std::forward<Range>( extensions ), ranges::back_inserter( m_extensions ) );
        return *this;
    }

    PhysicalDeviceSelector& withPresent( vk::SurfaceKHR surface ) &
    {
        assert( surface && "Empty surface handle" );
        m_surface = surface;
        return *this;
    }

    PhysicalDeviceSelector& withVersion( VulkanVersion version ) &
    {
        m_version = version;
        return *this;
    } // withVersion

    static constexpr auto k_default_types =
        std::array{ vk::PhysicalDeviceType::eDiscreteGpu, vk::PhysicalDeviceType::eIntegratedGpu };

    template <ranges::range Range = decltype( ( k_default_types ) )> // The second nested parenthesis here are crucial
    PhysicalDeviceSelector& withTypes( Range&& types = k_default_types ) &
    {
        // clang-format off
        auto zipped_view = ranges::views::zip_with( 
            []( auto&& a, auto&& b ) { return std::pair{ a, b }; }, 
            types, ranges::views::iota( 0 )
        ); // clang-format on

        m_types.clear();
        ranges::copy( zipped_view, ranges::inserter( m_types, m_types.end() ) );
        return *this;
    }

    auto make( vk::Instance instance ) const
    {
        auto predicate = [ & ]( auto&& elem ) -> bool {
            auto [ device, properties ] = elem;
            bool valid = physicalDeviceSupportsExtensions( device, m_extensions ).first &&
                m_types.contains( properties.deviceType );

            if ( m_surface )
            {
                valid = valid && physicalDeviceSupportsPresent( device, m_surface );
            }

            return valid;
        };

        auto all_devices = instance.enumeratePhysicalDevices();
        auto suitable = ranges::views::transform(
                            all_devices,
                            []( auto&& device ) {
                                return std::pair{ device, device.getProperties() };
                            } ) |
            ranges::views::filter( predicate ) | ranges::to_vector;

        ranges::sort( suitable, std::less{}, [ &types = std::as_const( m_types ) ]( auto&& pair ) {
            return types.at( pair.second.deviceType );
        } );

        return suitable;
    }
};

} // namespace vkwrap