/* This file contains definitions used by the most wrappers.
 * If you can't attribute something to a specific module you should put it here.
 */

#pragma once

#include <cstdint>

namespace vkwrap
{

enum class VulkanVersion : uint32_t
{
    e_version_1_0 = VK_MAKE_API_VERSION( 0, 1, 0, 0 ),
    e_version_1_1 = VK_MAKE_API_VERSION( 0, 1, 1, 0 ),
    e_version_1_2 = VK_MAKE_API_VERSION( 0, 1, 2, 0 ),
    e_version_1_3 = VK_MAKE_API_VERSION( 0, 1, 3, 0 ),
}; // VulkanVersion

// Call this to load instance/device independent functions. Do not call any vulkan function before calling this.
// Note[Sergei]: It is way more convenient to use default Dynamic Dispatch in <vulkan.hpp> instead of passing a separate
// loader everywhere.
void
initializeLoader();

} // namespace vkwrap

#include "vkwrap/error.h"
