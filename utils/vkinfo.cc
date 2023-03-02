/* Simple program that demostrates the usage of <vulkan.hpp> C++ wrappers.
 *
 */

#include "common/utility.h"
#include "common/vulkan_include.h"
#include "vkwrap/core.h"

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
};

} // namespace

int
main()
try
{
    using vkwrap::DebuggedInstance;
    using vkwrap::GenericInstance;

    auto counting_functor = std::make_shared<CountingCallback>();
    auto callback = [ counting_functor ]( auto sev, auto type, auto data ) -> bool // Capture by shared ptr by value
    {
        return counting_functor->operator()( sev, type, data );
    };

    auto instance = GenericInstance::make<DebuggedInstance>(
        vkwrap::VulkanVersion::e_version_1_3,
        nullptr,
        callback,
        std::to_array( { VK_EXT_DEBUG_UTILS_EXTENSION_NAME } ),
        std::to_array( { "VK_LAYER_KHRONOS_validation" } ) //
    );

    auto physical_devices = instance->enumeratePhysicalDevices();

    for ( auto&& device : physical_devices )
    {
        printPhysicalDeviceProperties( device );
    }

    fmt::print( "Number of callbacks = {}\n", counting_functor->m_call_count );
} catch ( vk::Error& e )
{
    fmt::print( "Vulkan error: {}\n", e.what() );
} catch ( std::exception& e )
{
    fmt::print( "Error: {}\n", e.what() );
}