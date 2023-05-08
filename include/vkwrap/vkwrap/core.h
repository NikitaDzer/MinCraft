/* This file contains definitions used by the most wrappers.
 * If you can't attribute something to a specific module you should put it here.
 */

#pragma once

#include "common/vulkan_include.h"

#include "utils/misc.h"

#include <spdlog/fmt/bundled/format.h>

#include <cstdint>
#include <string>

namespace vkwrap
{

enum class VulkanVersion : uint32_t
{
    e_version_1_0 = VK_MAKE_API_VERSION( 0, 1, 0, 0 ),
    e_version_1_1 = VK_MAKE_API_VERSION( 0, 1, 1, 0 ),
    e_version_1_2 = VK_MAKE_API_VERSION( 0, 1, 2, 0 ),
    e_version_1_3 = VK_MAKE_API_VERSION( 0, 1, 3, 0 ),
}; // VulkanVersion

inline std::string
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

inline std::string
versionToString( VulkanVersion version )
{
    return versionToString( utils::toUnderlying( version ) );
} // versionToString

using StringVector = std::vector<std::string>;

struct SupportsResult
{
    bool supports;
    StringVector missing;
};

// Call this to load instance/device independent functions. Do not call any vulkan function before calling this.
// Note[Sergei]: It is way more convenient to use default Dynamic Dispatch in <vulkan.hpp> instead of passing a separate
// loader everywhere.
void
initializeLoader();

using QueueIndex = uint32_t;
using QueueFamilyIndex = uint32_t;

} // namespace vkwrap

#include "vkwrap/error.h"
