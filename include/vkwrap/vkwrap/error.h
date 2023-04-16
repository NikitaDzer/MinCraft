#pragma once

#include "common/vulkan_include.h"
#include "utils/misc.h"

#include <spdlog/fmt/bundled/core.h>
#include <spdlog/fmt/bundled/format.h>

#include <cassert>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace vkwrap
{

class Error : public std::runtime_error
{

  public:
    Error( std::string msg )
        : std::runtime_error{ std::move( msg ) }
    {
    }

}; // Error

constexpr std::string_view
vulkanResultToStr( vk::Result result )
{
    using vk::Result;

    switch ( result )
    {
    // Provided by VK_VERSION_1_0
    case Result::eSuccess:
        return "Success";
    case Result::eNotReady:
        return "Not ready";
    case Result::eTimeout:
        return "Timeout";
    case Result::eEventSet:
        return "Event set";
    case Result::eEventReset:
        return "Event reset";
    case Result::eIncomplete:
        return "Incomplete";
    case Result::eErrorOutOfHostMemory:
        return "Error out of host memory";
    case Result::eErrorOutOfDeviceMemory:
        return "Error out of device memory";
    case Result::eErrorInitializationFailed:
        return "Error initialization failed";
    case Result::eErrorDeviceLost:
        return "Error device lost";
    case Result::eErrorMemoryMapFailed:
        return "Error memory map failed";
    case Result::eErrorLayerNotPresent:
        return "Error layer not present";
    case Result::eErrorExtensionNotPresent:
        return "Error extension not present";
    case Result::eErrorFeatureNotPresent:
        return "Error feature not present";
    case Result::eErrorIncompatibleDriver:
        return "Error incompatible driver";
    case Result::eErrorTooManyObjects:
        return "Error too many objects";
    case Result::eErrorFormatNotSupported:
        return "Error format not supported";
    case Result::eErrorFragmentedPool:
        return "Error fragmented pool";
    case Result::eErrorUnknown:
        return "Error unknown";
    // Provided by VK_KHR_surface
    case Result::eErrorSurfaceLostKHR:
        return "Error surface lost";
    case Result::eErrorNativeWindowInUseKHR:
        return "Error native window in use";
    // Provided by VK_KHR_swapchain
    case Result::eSuboptimalKHR:
        return "Error suboptimal";
    case Result::eErrorOutOfDateKHR:
        return "Error out of date";
    // Provided by VK_KHR_display_swapchain
    case Result::eErrorIncompatibleDisplayKHR:
        return "Error incompatible display";
    default:
        assert( 0 && "[Debug]: Unimplemented conversion from vk::Result to string." );
        std::terminate();
    }

} // vulkanResultToStr

class VulkanError : public Error
{
  private:
    vk::Result m_result;

  public:
    VulkanError( std::string msg, vk::Result result )
        : Error{ std::move( msg ) },
          m_result{ result }
    {
    }

    VulkanError( std::string msg, VkResult result )
        : Error{ std::move( msg ) },
          m_result{ vk::Result{ result } }
    {
    }

    vk::Result getResult() const { return m_result; }
    std::string_view getResultStr() const { return vulkanResultToStr( m_result ); }

}; // class VulkanError

enum class UnsupportedTag
{
    e_unsupported_extension,
    e_unsupported_layer,
}; // UnsupportedTag

constexpr std::string_view
unsupportedTagToStr( UnsupportedTag tag )
{
    switch ( tag )
    {
    case UnsupportedTag::e_unsupported_extension:
        return "Extension";
    case UnsupportedTag::e_unsupported_layer:
        return "Layer";
    default:
        assert( 0 && "[Debug]: Broken UnsupportedTag enum" );
        std::terminate();
    }
} // unsupportedTagToStr

struct UnsupportedEntry
{
    UnsupportedTag tag;
    std::string name;
}; // UnsupportedEntry

class UnsupportedError : public Error, private std::vector<UnsupportedEntry>
{
  private:
    using VectorBase = std::vector<UnsupportedEntry>;

  public:
    UnsupportedError( std::string msg, VectorBase missing = {} )
        : Error{ std::move( msg ) },
          VectorBase{ std::move( missing ) }
    {
    }

    using VectorBase::cbegin;
    using VectorBase::cend;
    using VectorBase::empty;
    using VectorBase::size;
    using VectorBase::operator[];

    auto begin() const { return cbegin(); }
    auto end() const { return cend(); }
}; // UnsupportedError

} // namespace vkwrap
