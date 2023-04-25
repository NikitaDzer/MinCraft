/* This file contains definitions used by the most wrappers.
 * If you can't attribute something to a specific module you should put it here.
 */

#pragma once

#include "common/vulkan_include.h"

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

class Weight
{
  public:
    constexpr Weight( int value = 0 )
        : m_value{ value }
    {
    } // Weight

    static constexpr int k_bad_value = -1;

    constexpr int value() const { return m_value; }
    constexpr operator bool() const { return m_value >= 0; }
    constexpr auto operator<=>( const Weight& ) const = default;

  public:
    Weight& operator+=( const Weight& rhs )
    {
        m_value = ( ( *this && rhs ) ? ( m_value + rhs.m_value ) : k_bad_value );
        return *this;
    } // operator+=

    friend Weight operator+( const Weight& lhs, const Weight& rhs )
    {
        auto tmp = Weight{ lhs };
        tmp += rhs;
        return tmp;
    } // operator+

  private:
    int m_value = k_bad_value;

}; // class Weight

static constexpr auto k_bad_weight = Weight{ Weight::k_bad_value };

} // namespace vkwrap

#include "vkwrap/error.h"
