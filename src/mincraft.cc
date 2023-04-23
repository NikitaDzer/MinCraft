/* Simple program that demostrates the usage of <vulkan.hpp> C++ wrappers.
 *
 */

#include "common/vulkan_include.h"

#include "vkwrap/command.h"
#include "vkwrap/device.h"
#include "vkwrap/instance.h"
#include "vkwrap/pipeline.h"
#include "vkwrap/queues.h"

#include "glfw/window.h"

#include <spdlog/cfg/env.h>
#include <spdlog/fmt/bundled/core.h>
#include <spdlog/fmt/bundled/format.h>

#include <popl/popl.hpp>

#include <range/v3/algorithm/any_of.hpp>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <iterator>
#include <numeric>
#include <sstream>
#include <string>

namespace
{

struct DebugCallback
{
    std::atomic<unsigned> call_count = 0;
    bool operator()(
        vk::DebugUtilsMessageSeverityFlagBitsEXT sev,
        vk::DebugUtilsMessageTypeFlagsEXT type,
        const vk::DebugUtilsMessengerCallbackDataEXT& data )
    {
        ++call_count;
        return vkwrap::defaultDebugCallback( sev, type, data );
    }
}; // CountingCallback

constexpr auto k_vulkan_version = vkwrap::VulkanVersion::e_version_1_3;

struct CreateInstanceResult
{
    vkwrap::GenericInstance instance;
    std::shared_ptr<DebugCallback> callback;
};

CreateInstanceResult
createInstance( const glfw::Instance& glfw_instance, bool validation )
{
    auto instance_builder = vkwrap::InstanceBuilder{};
    instance_builder.withVersion( k_vulkan_version ).withExtensions( glfw_instance.getRequiredExtensions() );

    auto make_instance = [ &instance_builder ]() {
        auto instance = instance_builder.make();
        VULKAN_HPP_DEFAULT_DISPATCHER.init( instance.get() );
        return instance;
    };

    if ( validation )
    {
        auto functor = std::make_shared<DebugCallback>();
        auto callback = [ functor ]( auto&& sev, auto&& type, auto&& data ) -> bool // Capture by shared ptr by value
        {
            return functor->operator()( sev, type, data );
        };

        instance_builder.withDebugMessenger().withValidationLayers().withCallback( callback );
        return CreateInstanceResult{ make_instance(), functor };
    }

    return CreateInstanceResult{ make_instance(), nullptr };
}

struct AppOptions
{
    bool validation = false;
};

std::optional<AppOptions>
parseOptions( std::span<const char*> command_line_args )
{
    popl::OptionParser op( "Allowed options" );

    auto help_option = op.add<popl::Switch>( "h", "help", "Print this help message" );
    auto validation_option = op.add<popl::Switch>( "d", "debug", "" );

    op.parse( command_line_args.size(), command_line_args.data() );

    if ( help_option->is_set() )
    {
        fmt::print( "{}", op.help() );
        return std::nullopt;
    }

#ifndef NDEBUG
    bool validation = true; // Enable validation layers in debug build
#else
    bool validation = validation_option->is_set();
#endif

    return AppOptions{ .validation = validation };
}

vkwrap::PhysicalDevice
pickPhysicalDevice( vk::Instance instance, vk::SurfaceKHR surface )
{
    vkwrap::PhysicalDeviceSelector physical_selector;

    auto depth_weight = []( vkwrap::PhysicalDeviceInfo info ) -> int {
        auto candidates = std::to_array<vk::Format>(
            { vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint } );

        auto is_compatible =
            ranges::any_of( candidates, [ physical_device = info.device ]( vk::Format format ) -> bool {
                auto props = physical_device.getFormatProperties( format );
                return ( props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment ) ==
                    vk::FormatFeatureFlagBits::eDepthStencilAttachment;
            } );

        return ( is_compatible ? 0 : -1 );
    };

    // Calculate weights depending on the requirements. Should be expanded to check for Format capabilities e.t.c. Help
    // needed :)
    auto weight_functor = [ depth_weight ]( vkwrap::PhysicalDeviceInfo info ) {
        return depth_weight( info );
    };

    physical_selector.withExtensions( std::array{ VK_KHR_SWAPCHAIN_EXTENSION_NAME } )
        .withTypes( std::array{ vk::PhysicalDeviceType::eDiscreteGpu, vk::PhysicalDeviceType::eIntegratedGpu } )
        .withPresent( surface )
        .withVersion( vkwrap::VulkanVersion::e_version_1_3 )
        .withWeight( weight_functor );

    auto suitable_devices = physical_selector.make( instance );

    if ( suitable_devices.empty() )
    {
        throw std::runtime_error{ "No suitable physical devices found" };
    }

    return suitable_devices.at( 0 ).device;
}

struct LogicalDeviceCreateResult
{
    vkwrap::LogicalDevice device;
    vkwrap::Queue graphics;
    vkwrap::Queue present;
};

LogicalDeviceCreateResult
createLogicalDeviceQueues( vk::PhysicalDevice physical_device, vk::SurfaceKHR surface )
{
    vkwrap::Queue graphics;
    vkwrap::Queue present;
    vkwrap::LogicalDeviceBuilder device_builder;

    device_builder.withExtensions( std::array{ VK_KHR_SWAPCHAIN_EXTENSION_NAME } )
        .withGraphicsQueue( graphics )
        .withPresentQueue( surface, present );

    auto logical_device = device_builder.make( physical_device );
    VULKAN_HPP_DEFAULT_DISPATCHER.init( logical_device.get() );

    return LogicalDeviceCreateResult{ .device = std::move( logical_device ), .graphics = graphics, .present = present };
}

void
runApplication( std::span<const char*> command_line_args )
{
    AppOptions options;

    if ( auto parsed = parseOptions( command_line_args ); !parsed )
    {
        return;
    } else
    {
        options = parsed.value();
    }

    spdlog::cfg::load_env_levels();
    // Use `export SPDLOG_LEVEL=debug` to set maximum logging level
    // Or `export SPDLOG_LEVEL=warn` to print only warnings and errors
    vkwrap::initializeLoader(); // Load basic functions that are instance independent

    auto glfw_instance = glfw::Instance{};
    auto window = glfw::wnd::Window{ glfw::wnd::WindowConfig{ .title = "MinCraft" } };

    auto vk_instance = createInstance( glfw_instance, options.validation ).instance;
    auto surface = window.createSurface( vk_instance );
    auto physical_device = pickPhysicalDevice( vk_instance, surface.get() );

    auto [ logical_device, graphics_queue, present_queue ] =
        createLogicalDeviceQueues( physical_device.get(), surface.get() );

    auto command_pool = vkwrap::CommandPool{ logical_device, graphics_queue };
    auto one_time_cmd = vkwrap::OneTimeCommand{ command_pool, graphics_queue.get() };

    while ( window.running() )
    {
        glfwPollEvents();
    }
}

} // namespace

int
main( int argc, const char** argv )
try
{
    runApplication( std::span{ argv, static_cast<size_t>( argc ) } );
} catch ( vkwrap::UnsupportedError& e )
{
    fmt::print( "Unsupported error: {}\n", e.what() );
    for ( unsigned i = 0; auto&& entry : e )
    {
        fmt::print( "[{}]. {}: {}\n", i++, vkwrap::unsupportedTagToStr( entry.tag ), entry.name );
    }
} catch ( vk::Error& e )
{
    fmt::print( "Vulkan error: {}\n", e.what() );
} catch ( std::exception& e )
{
    fmt::print( "Error: {}\n", e.what() );
}
