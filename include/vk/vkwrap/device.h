#pragma once

#include "common/utils.h"
#include "common/vulkan_include.h"

#include "vkwrap/core.h"
#include "vkwrap/surface.h"

#include "range/v3/range/conversion.hpp"
#include <range/v3/algorithm/copy.hpp>
#include <range/v3/algorithm/sort.hpp>
#include <range/v3/iterator/insert_iterators.hpp>
#include <range/v3/range/concepts.hpp>
#include <range/v3/view/all.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/iota.hpp>
#include <range/v3/view/zip.hpp>
#include <range/v3/view/zip_with.hpp>

#include <cassert>
#include <cstddef>
#include <functional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace vkwrap
{

template <ranges::range Range>
std::pair<bool, std::vector<std::string>>
physicalDeviceSupportsExtensions( vk::PhysicalDevice physical_device, Range&& extensions )
{
    auto supports = physical_device.enumerateDeviceExtensionProperties();
    auto missing =
        utils::findAllMissing( supports, std::forward<Range>( extensions ), &vk::ExtensionProperties::extensionName );
    return std::pair{ missing.empty(), missing };
}

struct PhysicalDevicePropertiesPair
{
    vk::PhysicalDevice device;
    vk::PhysicalDeviceProperties properties;
};

inline bool
operator==( const PhysicalDevicePropertiesPair& lhs, const PhysicalDevicePropertiesPair& rhs )
{
    return lhs.device == rhs.device;
}

inline bool
operator!=( const PhysicalDevicePropertiesPair& lhs, const PhysicalDevicePropertiesPair& rhs )
{
    return lhs.device != rhs.device;
}

} // namespace vkwrap

namespace std
{

template <> struct hash<vkwrap::PhysicalDevicePropertiesPair>
{
    size_t operator()( const vkwrap::PhysicalDevicePropertiesPair& pair ) const
    {
        auto&& [ _, prop ] = pair;
        size_t seed = 0;
        utils::hashCombine( seed, prop.deviceID );
        utils::hashCombine( seed, prop.vendorID );
        return seed;
    }
};

} // namespace std

namespace vkwrap
{

class PhysicalDeviceSelector
{
  public:
    using WeightFunction = std::function<int( PhysicalDevicePropertiesPair )>;

  private:
    std::vector<std::string> m_extensions;                   // Extensions that the device needs to support
    std::unordered_map<vk::PhysicalDeviceType, int> m_types; // A range of device types to prefer
    vk::SurfaceKHR m_surface;                                // Surface to present to

    VulkanVersion m_version = VulkanVersion::e_version_1_0; // Version the physical device has to support
    WeightFunction m_weight_function = []( auto&& ) -> int {
        return 0;
    };

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

    PhysicalDeviceSelector& withWeight( WeightFunction function ) &
    {
        assert( function );
        m_weight_function = function;
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

  private:
    auto calculateWeight( const PhysicalDevicePropertiesPair& elem ) const
        -> std::pair<PhysicalDevicePropertiesPair, int>
    {
        auto [ device, properties ] = elem;
        constexpr int invalid = -1;

        // Call custom function to skip doing unnecessary work if possible.
        auto weight = m_weight_function( elem );

        // Check basic requirements.
        bool valid = ( weight >= 0 ) && physicalDeviceSupportsExtensions( device, m_extensions ).first &&
            m_types.contains( properties.deviceType );

        if ( m_surface )
        {
            valid = valid && physicalDeviceSupportsPresent( device, m_surface );
        }

        int cost = valid ? weight + m_types.at( properties.deviceType ) : invalid;
        return std::pair{ elem, cost };
    }

    using WeightMap = std::unordered_map<PhysicalDevicePropertiesPair, int>;

    WeightMap makeWeighted( const vk::Instance& instance ) const
    {
        const auto weight_calculation = [ & ]( auto&& elem ) {
            return calculateWeight( elem );
        };

        auto all_devices = instance.enumeratePhysicalDevices();
        WeightMap weight_map;

        const auto get_properites = []( auto&& device ) {
            return PhysicalDevicePropertiesPair{ device, device.getProperties() };
        };

        auto views = ranges::views::all( all_devices ) | ranges::views::transform( get_properites ) |
            ranges::views::transform( weight_calculation ) |
            ranges::views::filter( []( auto&& elem ) { return elem.second >= 0; } );

        ranges::copy( views, ranges::inserter( weight_map, weight_map.end() ) );

        return weight_map;
    }

  public:
    auto make( const vk::Instance& instance ) const
    {
        auto weight_map = makeWeighted( instance );

        auto suitable =
            ranges::views::transform( weight_map, []( auto&& elem ) { return elem.first; } ) | ranges::to_vector;

        ranges::sort( suitable, std::less{}, [ &weights = std::as_const( weight_map ) ]( auto&& pair ) {
            return weights.at( pair );
        } );

        return suitable;
    }
};

} // namespace vkwrap