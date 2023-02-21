/* <vulkan_inlcude.h>
 * Include this file anywhere you would include <vulkan.hpp> header.
 * Define additional preprocessor flags only in this file. Otherwise you run the
 * possibility of mismatched class definitions, which is UB.
 */

#pragma once

#define VULKAN_HPP_NO_CONSTRUCTORS // For C++20 designated initializers
#include <vulkan/vulkan.hpp>
