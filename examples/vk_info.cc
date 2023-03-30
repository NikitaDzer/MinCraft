/* Simple program that demostrates the usage of <vulkan.hpp> C++ wrappers.
 *
 */

#include "common/vulkan_include.h"

#include "vkwrap/device.h"
#include "vkwrap/instance.h"
#include "vkwrap/queues.h"

#include "window/window.h"

#include <spdlog/cfg/env.h>
#include <spdlog/fmt/bundled/core.h>

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <sstream>
#include <string>

namespace
{

std::string
versionToString( uint32_t version )
{
    const auto ver_major = VK_VERSION_MAJOR( version );
    const auto ver_minor = VK_VERSION_MINOR( version );
    const auto ver_patch = VK_VERSION_PATCH( version );

    return fmt::format(
        "{}.{}.{}",
        ver_major,
        ver_minor,
        ver_patch // This is a workaround for clang-format
    );
} // versionToString

void
printPhysicalDeviceProperties( vk::PhysicalDevice device )
{
    const auto extensions = device.enumerateDeviceExtensionProperties();
    const auto properties = device.getProperties();

    fmt::print(
        "Found physical device: [id = {}, type = {}, name = {}, version = {}]\n",
        properties.deviceID,
        vk::to_string( properties.deviceType ),
        properties.deviceName,
        versionToString( properties.apiVersion ) //
    );

    const auto extensions_string = std::accumulate(
        extensions.begin(),
        extensions.end(),
        std::string{},
        []( auto str, auto ext ) {
            return str + " " + ext.extensionName.data();
        } //
    );

    fmt::print(
        "Physical device [{}] supports following extensions: {}\n",
        properties.deviceName,
        extensions_string //
    );

    fmt::print( "Physical device [{}] has following queue families\n", properties.deviceName );
    const auto qf_info = device.getQueueFamilyProperties();
    for ( uint32_t i = 0; const auto& info : qf_info )
    {
        fmt::print(
            "[{}]. [queue_count = {}] of the type {}\n",
            i,
            info.queueCount,
            vk::to_string( info.queueFlags ) //
        );

        ++i;
    }
} // printPhysicalDeviceProperties

struct CountingCallback
{
    unsigned m_call_count = 0;
    bool operator()(
        vk::DebugUtilsMessageSeverityFlagBitsEXT sev,
        vk::DebugUtilsMessageTypeFlagsEXT type,
        const vk::DebugUtilsMessengerCallbackDataEXT& data )
    {
        m_call_count++;
        return vkwrap::defaultDebugCallback( sev, type, data );
    }
}; // CountingCallback

} // namespace

int
main()
try
{
    using vkwrap::DebuggedInstance;
    using vkwrap::GenericInstance;

    spdlog::cfg::load_env_levels();
    // Use `export SPDLOG_LEVEL=debug` to set maximum logging level
    // Or `export SPDLOG_LEVEL=warn` to print only warnings and errors

    vkwrap::initializeLoader(); // Load basic functions that are instance independent
    using wnd::glfw::Window;
    using wnd::glfw::WindowManager;

    WindowManager::initialize();

    auto counting_functor = std::make_shared<CountingCallback>();
    auto callback =
        [ counting_functor ]( auto&& sev, auto&& type, auto&& data ) -> bool // Capture by shared ptr by value
    {
        return counting_functor->operator()( sev, type, data );
    };

    auto instance_builder = vkwrap::InstanceBuilder{};
    instance_builder.withVersion( vkwrap::VulkanVersion::e_version_1_3 )
        .withDebugMessenger()
        .withValidationLayers()
        .withExtensions( WindowManager::getRequiredExtensions() )
        .withCallback( callback );

    // This assert is for testing purposes.
    const auto layers = std::array{ "VK_LAYER_KHRONOS_validation" }; // Initializer list
    assert( DebuggedInstance::supportsLayers( layers ).supports && "Instance does not support validation layers" );
    auto instance = instance_builder.make();
    assert( instance && "Checking that instance was actually created" );

    auto physical_devices = instance->enumeratePhysicalDevices();
    for ( auto&& device : physical_devices )
    {
        printPhysicalDeviceProperties( device );
    }

    fmt::print( "\n" );

    auto window = Window{ { .title = "Test window" } };
    auto surface = window.createSurface( instance.get() );

    vkwrap::PhysicalDeviceSelector physical_selector;
    physical_selector.withExtensions( std::array{ VK_KHR_SWAPCHAIN_EXTENSION_NAME } )
        .withTypes( { vk::PhysicalDeviceType::eDiscreteGpu, vk::PhysicalDeviceType::eIntegratedGpu } )
        .withPresent( surface.get() )
        .withVersion( vkwrap::VulkanVersion::e_version_1_3 );

    // Sorted vector of
    auto suitable_devices = physical_selector.make( instance );
    auto physical_device = suitable_devices.at( 0 ).device;

    vkwrap::Queue graphics;
    vkwrap::Queue present;
    vkwrap::LogicalDeviceBuilder device_builder;

    auto logical_device = device_builder.withGraphicsQueue( graphics )
                              .withPresentQueue( surface.get(), present )
                              .make( physical_device.get() );

    fmt::print( "Graphics == Present: {}\n", graphics == present );
    fmt::print( "Found physical devices:\n" );

    for ( unsigned i = 0; auto&& pair : suitable_devices )
    {
        auto&& [ device, info ] = pair;
        fmt::print( "[{}]. name = {}, type = {}\n", i++, info.deviceName, vk::to_string( info.deviceType ) );
    }

    fmt::print( "\nNumber of callbacks = {}\n", counting_functor->m_call_count );

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
