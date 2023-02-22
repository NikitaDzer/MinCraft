/* Simple program that demostrates the usage of <vulkan.hpp> C++ wrappers.
 *
 */

#include <fmt/core.h>

#include <algorithm>
#include <cstdint>
#include <numeric>
#include <sstream>
#include <string>

#include "unified_includes/vulkan_include.h"

namespace
{

constexpr auto k_application_info = vk::ApplicationInfo{
    .pApplicationName = "vkinfo",
    .applicationVersion = VK_MAKE_VERSION( 0, 0, 1 ),
    .pEngineName = "no engine",
    .engineVersion = VK_MAKE_VERSION( 0, 0, 1 ),
    .apiVersion = VK_VERSION_1_1 };

std::string
versionToString ( uint32_t version )
{
    const auto ver_major = VK_VERSION_MAJOR( version );
    const auto ver_minor = VK_VERSION_MINOR( version );
    const auto ver_patch = VK_VERSION_PATCH( version );
    return fmt::format( "{}.{}.{}", ver_major, ver_minor, ver_patch );
}

void
printPhysicalDeviceProperties ( vk::PhysicalDevice device )
{
    const auto extensions = device.enumerateDeviceExtensionProperties();
    const auto properties = device.getProperties();

    fmt::println(
        "Found physical device: [id = {}, type = {}, name = {}, version = {}]",
        properties.deviceID,
        vk::to_string( properties.deviceType ),
        properties.deviceName,
        versionToString( properties.apiVersion )
    );

    const auto extensions_string =
        std::accumulate( extensions.begin(), extensions.end(), std::string{}, [] ( auto str, auto ext ) {
            return str + " " + ext.extensionName.data();
        } );

    fmt::println( "Device [{}] supports following extensions: {}", properties.deviceName, extensions_string );
}

} // namespace

int
main ()
try
{
    const auto instance_create_info = vk::InstanceCreateInfo{ .pApplicationInfo = &k_application_info };
    auto instance = vk::createInstanceUnique( instance_create_info );

    const auto physical_devices = instance->enumeratePhysicalDevices();
    for ( const auto& device : physical_devices )
    {
        printPhysicalDeviceProperties( device );
    }
} catch ( vk::Error& e )
{
    fmt::println( "Vulkan error: {}", e.what() );
} catch ( std::exception& e )
{
    fmt::println( "Error: {}", e.what() );
}