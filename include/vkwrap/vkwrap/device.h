#pragma once

#include "common/vulkan_include.h"

#include "utils/algorithm.h"
#include "utils/misc.h"
#include "utils/range.h"

#include "vkwrap/core.h"
#include "vkwrap/queues.h"
#include "vkwrap/surface.h"

#include <range/v3/view/set_algorithm.hpp>
#include <range/v3/view/take.hpp>
#include <range/v3/view/transform.hpp>

#include <range/v3/algorithm/copy.hpp>
#include <range/v3/algorithm/set_algorithm.hpp>
#include <range/v3/algorithm/sort.hpp>

#include "range/v3/range/conversion.hpp"
#include <range/v3/iterator/insert_iterators.hpp>
#include <range/v3/range/concepts.hpp>

#include <range/v3/view/all.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/iota.hpp>
#include <range/v3/view/zip.hpp>
#include <range/v3/view/zip_with.hpp>

#include <boost/functional/hash.hpp>

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
SupportsResult
physicalDeviceSupportsExtensions( const vk::PhysicalDevice& physical_device, Range&& extensions )
{
    assert( physical_device );

    auto supports = physical_device.enumerateDeviceExtensionProperties();
    auto missing =
        utils::findAllMissing( supports, std::forward<Range>( extensions ), &vk::ExtensionProperties::extensionName );

    return SupportsResult{ missing.empty(), missing };
} // physicalDeviceSupportsExtensions

inline uint32_t
physicalDeviceQueueFamilyCount( const vk::PhysicalDevice& physical_device )
{
    assert( physical_device );

    QueueFamilyIndex queue_family_count = 0;
    physical_device.getQueueFamilyProperties(
        &queue_family_count,
        nullptr //
    );

    return queue_family_count;
} // physicalDeviceQueueFamilyCount

class PhysicalDevice : private vk::PhysicalDevice
{
  public:
    PhysicalDevice( const vk::PhysicalDevice& physical_device )
        : vk::PhysicalDevice{ physical_device }
    {
    }

    const vk::PhysicalDevice& get() const& { return *this; }

    template <ranges::range Range> SupportsResult supportsExtensions( Range&& extensions ) const
    {
        return physicalDeviceSupportsExtensions( *this, std::forward<Range>( extensions ) );
    } // supportsExtensions

    uint32_t queueFamilyCount() const { return physicalDeviceQueueFamilyCount( *this ); }

    using vk::PhysicalDevice::getFeatures;
    using vk::PhysicalDevice::operator bool;

    bool operator==( const PhysicalDevice& other ) const = default;
}; // PhysicalDevice

struct PhysicalDeviceInfo
{
    PhysicalDevice device;
    vk::PhysicalDeviceProperties properties;
    vk::PhysicalDeviceIDProperties identifier;

    bool operator==( const PhysicalDeviceInfo& rhs ) const = default;
};

} // namespace vkwrap

namespace std
{

template <> struct hash<vkwrap::PhysicalDeviceInfo>
{
    size_t operator()( const vkwrap::PhysicalDeviceInfo& info ) const
    {
        size_t seed = 0;

        for ( auto&& uid : info.identifier.deviceUUID )
        {
            boost::hash_combine( seed, uid );
        }

        return seed;
    }
}; // hash

} // namespace std

namespace vkwrap
{

class Weight
{

  private:
    int m_value;

  public:
    Weight( int value )
        : m_value{ value }
    {
    } // Weight( int )

    static constexpr int k_bad_weight = -1;

    int value() const { return m_value; }

    // clang-format off
    operator bool() const { return m_value > k_bad_weight; }
    operator int()  const { return m_value; }
    // clang-format on

    constexpr auto operator<=>( const Weight& ) const = default;
    Weight& operator+=( const Weight& additional )
    {
        if ( *this && additional )
        {
            int new_value = m_value + additional.m_value;

            assert( new_value >= 0 );
            m_value = new_value;
        } else
        {
            m_value = Weight::k_bad_weight;
        }

        return *this;
    } // operator+=( const Weight& )

    friend Weight operator+( const Weight& lhs, const Weight& rhs )
    {
        Weight tmp{ lhs };
        return tmp += rhs;
    } // friend operator+( const Weight&, const Weight& )

}; // class Weight

class PhysicalDeviceSelector
{
  public:
    using WeightFunction = std::function<Weight( PhysicalDeviceInfo )>;

  private:
    struct WeightPhysicalDeviceInfo
    {
        PhysicalDeviceInfo info;
        Weight weight;
    }; // struct WeightPhysicalDeviceInfo

  private:
    StringVector m_extensions;                                  // Extensions that the device needs to support
    std::unordered_map<vk::PhysicalDeviceType, Weight> m_types; // A range of device types to prefer

    VulkanVersion m_version = VulkanVersion::e_version_1_0; // Version the physical device has to support
    WeightFunction m_weight_function = []( auto&& ) -> Weight {
        return 0;
    };

  public:
    template <ranges::range Range> PhysicalDeviceSelector& withExtensions( Range&& extensions ) &
    {
        ranges::copy( std::forward<Range>( extensions ), ranges::back_inserter( m_extensions ) );
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
    } // withTypes

  private:
    WeightPhysicalDeviceInfo calculateWeight( const PhysicalDeviceInfo& elem ) const
    {
        auto [ device, properties, id ] = elem;

        // Call custom function to skip doing unnecessary work if possible.
        auto weight = m_weight_function( elem );

        // Check basic requirements.
        if ( !device.supportsExtensions( m_extensions ).supports )
        {
            weight = Weight::k_bad_weight;
        }

        if ( !m_types.contains( properties.deviceType ) )
        {
            weight = Weight::k_bad_weight;
        }

        weight += m_types.at( properties.deviceType );
        return { elem, weight };
    } // calculateWeight

    std::vector<WeightPhysicalDeviceInfo> makeWeighted( vk::Instance instance ) const
    {
        const auto weight_calculation = [ & ]( auto&& elem ) {
            return calculateWeight( elem );
        };

        auto all_devices = instance.enumeratePhysicalDevices();

        const auto get_properites = []( const vk::PhysicalDevice& device ) {
            auto chain = device.getProperties2<vk::PhysicalDeviceProperties2, vk::PhysicalDeviceIDProperties>();

            auto properties = chain.get<vk::PhysicalDeviceProperties2>().properties;
            auto id = chain.get<vk::PhysicalDeviceIDProperties>();

            return PhysicalDeviceInfo{ device, properties, id };
        };

        auto views = ranges::views::all( all_devices ) | ranges::views::transform( get_properites ) |
            ranges::views::transform( weight_calculation ) |
            ranges::views::filter( []( auto&& elem ) { return elem.weight; } );

        return ranges::to_vector( views );
    } // makeWeighted

  public:
    auto make( vk::Instance instance ) const
    {
        auto suitable = makeWeighted( instance );

        // Sort by ascending weight.
        ranges::sort( suitable, []( auto&& first, auto&& second ) { return first.weight < second.weight; } );

        return suitable;
    } // make
};    // PhysicalDeviceSelector

class LogicalDevice : private vk::UniqueDevice
{
  private:
    using Base = vk::UniqueDevice;

  public:
    using Base::operator bool;
    using Base::operator->;
    using Base::get;

    LogicalDevice( vk::UniqueDevice logical_device )
        : Base{ std::move( logical_device ) }
    {
    }

    operator vk::Device() const { return get(); }
}; // LogicalDevice

class LogicalDeviceBuilder
{
  private:
    struct GraphicsQueueCreateData
    {
        Queue* queue = nullptr;
    }; // GraphicsQueueCreateData

    struct PresentQueueCreateData
    {
        Queue* queue = nullptr;
        vk::SurfaceKHR surface = nullptr; // Surface to present to.
    };                                    // PresentQueueCreateData

  private:
    PresentQueueCreateData m_present_queue_data;
    GraphicsQueueCreateData m_graphics_queue_data;

    StringVector m_extensions;
    vk::PhysicalDeviceFeatures m_features;

  public:
    LogicalDeviceBuilder& withFeatures( const vk::PhysicalDeviceFeatures& features ) &
    {
        m_features = features;
        return *this;
    } // withFeatures

    LogicalDeviceBuilder& withGraphicsQueue( Queue& queue ) &
    {
        m_graphics_queue_data = GraphicsQueueCreateData{ &queue };
        return *this;
    } // withGraphicsQueue

    LogicalDeviceBuilder& withPresentQueue( const vk::SurfaceKHR& surface, Queue& queue ) &
    {
        m_present_queue_data = PresentQueueCreateData{ &queue, surface };
        return *this;
    } // withPresentQueue

    template <ranges::range Range> LogicalDeviceBuilder& withExtensions( Range&& extensions ) &
    {
        ranges::copy( std::forward<Range>( extensions ), ranges::back_inserter( m_extensions ) );
        return *this;
    } // withExtensions

  private:
    std::vector<QueueFamilyIndex> getPresentFamilies( const vk::PhysicalDevice& physical_device ) const
    {
        auto queue_indices = ranges::views::iota( QueueFamilyIndex{ 0 } ) |
            ranges::views::take( physicalDeviceQueueFamilyCount( physical_device ) );

        auto predicate = [ &physical_device, &surface = m_present_queue_data.surface ]( uint32_t index ) -> bool {
            return physical_device.getSurfaceSupportKHR( index, surface );
        };

        return queue_indices | ranges::views::filter( predicate ) | ranges::to_vector;
    } // getPresentFamilies

    std::vector<QueueFamilyIndex> getGraphicsFamilies( const vk::PhysicalDevice& physical_device ) const
    {
        auto queue_indices = ranges::views::iota( QueueFamilyIndex{ 0 } ) |
            ranges::views::take( physicalDeviceQueueFamilyCount( physical_device ) );

        auto predicate = []( auto&& pair ) -> bool {
            vk::QueueFamilyProperties properties = pair.second;
            return utils::toBool( properties.queueFlags & vk::QueueFlagBits::eGraphics );
        };

        auto queue_family_properties = physical_device.getQueueFamilyProperties();

        return ranges::views::zip( queue_indices, queue_family_properties ) | ranges::views::filter( predicate ) |
            ranges::views::transform( []( auto&& pair ) { return pair.first; } ) | ranges::to_vector;
    } // getGraphicsFamilies

  public:
    LogicalDevice make( const vk::PhysicalDevice& physical_device ) const
    {
        auto present_families = getPresentFamilies( physical_device );
        auto graphics_families = getGraphicsFamilies( physical_device );

        auto cstr_extensions = utils::convertToCStrVector( m_extensions );

        std::vector<vk::DeviceQueueCreateInfo> requested_queues;
        uint32_t g_qfi = 0, p_qfi = 0;
        auto both_families = ranges::views::set_intersection( present_families, graphics_families ) | ranges::to_vector;

        auto queue_priorities = std::array{ 1.0f }; // Default priority. 1.0f indicates maximum priority.
        auto basic_queue_create_info =
            vk::DeviceQueueCreateInfo{ .queueCount = 1, .pQueuePriorities = queue_priorities.data() };

        bool should_make_present = m_present_queue_data.queue != nullptr;
        bool should_make_graphics = m_graphics_queue_data.queue != nullptr;

        if ( ( present_families.empty() && should_make_present ) ||
             ( graphics_families.empty() && should_make_graphics ) )
        {
            throw Error{ "Not all required queues are supported" };
        }

        if ( both_families.size() ) // Graphics family supports present.
        {
            // Don't care which exact family. For now. Maybe should choose a family with the
            // most queues, e.t.c. But it's unlikely that we will need to do that.
            g_qfi = p_qfi = both_families.front();
            basic_queue_create_info.setQueueFamilyIndex( g_qfi );
            requested_queues.push_back( basic_queue_create_info );
        } else
        {
            g_qfi = graphics_families.front();
            p_qfi = present_families.front();

            basic_queue_create_info.setQueueFamilyIndex( g_qfi );
            requested_queues.push_back( basic_queue_create_info );
            basic_queue_create_info.setQueueFamilyIndex( p_qfi );
            requested_queues.push_back( basic_queue_create_info );
        }

        auto device_create_info = vk::DeviceCreateInfo{
            .queueCreateInfoCount = static_cast<uint32_t>( requested_queues.size() ),
            .pQueueCreateInfos = requested_queues.data(),
            .enabledExtensionCount = static_cast<uint32_t>( cstr_extensions.size() ),
            .ppEnabledExtensionNames = cstr_extensions.data(),
            .pEnabledFeatures = &m_features };

        LogicalDevice device = physical_device.createDeviceUnique( device_create_info );

        if ( auto* g_ptr = m_graphics_queue_data.queue; g_ptr )
        {
            *g_ptr = Queue{ device.get(), g_qfi, 0 };
        }

        if ( auto* p_ptr = m_present_queue_data.queue; p_ptr )
        {
            *p_ptr = Queue{ device.get(), p_qfi, 0 };
        }

        return device;
    } // make
};    // LogicalDeviceBuilder

} // namespace vkwrap
