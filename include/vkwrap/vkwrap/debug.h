#pragma once

#include "common/vulkan_include.h"

#include "vkwrap/core.h"

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace vkwrap
{

using MsgSev = vk::DebugUtilsMessageSeverityFlagBitsEXT;
using MsgType = vk::DebugUtilsMessageTypeFlagBitsEXT;

static constexpr auto k_default_severity_flags = MsgSev::eVerbose | MsgSev::eWarning | MsgSev::eError | MsgSev::eInfo;
static constexpr auto k_default_type_flags = MsgType::eGeneral | MsgType::eValidation | MsgType::ePerformance;

using DebugUtilsCallbackSignature = bool(
    vk::DebugUtilsMessageSeverityFlagBitsEXT,
    vk::DebugUtilsMessageTypeFlagsEXT,
    const vk::DebugUtilsMessengerCallbackDataEXT& );

using DebugUtilsCallbackFunctionType = std::function<DebugUtilsCallbackSignature>;

std::string
assembleDebugMessage(
    vk::DebugUtilsMessageTypeFlagsEXT message_types,
    const vk::DebugUtilsMessengerCallbackDataEXT& data );

bool
defaultDebugCallback(
    vk::DebugUtilsMessageSeverityFlagBitsEXT message_severity,
    vk::DebugUtilsMessageTypeFlagsEXT message_types,
    const vk::DebugUtilsMessengerCallbackDataEXT& callback_data );

struct DebugMessengerConfig
{
    DebugUtilsCallbackFunctionType m_callback_func = defaultDebugCallback;
    vk::DebugUtilsMessageSeverityFlagsEXT m_severity_flags = k_default_severity_flags;
    vk::DebugUtilsMessageTypeFlagsEXT m_type_flags = k_default_type_flags;
}; // DebugMessengerConfig

class DebugMessenger
{
  public:
  private:
    using HandleType = vk::UniqueHandle<vk::DebugUtilsMessengerEXT, vk::DispatchLoaderDynamic>;

    std::unique_ptr<DebugUtilsCallbackFunctionType> m_callback; // Here unqiue ptr is needed so that if the object is
                                                                // moved from, then user_data pointer does not change.
    HandleType m_handle;

  private:
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
        VkDebugUtilsMessageTypeFlagsEXT message_types,
        const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
        void* user_data );

  public:
    DebugMessenger() = default;

    DebugMessenger( vk::Instance instance, DebugMessengerConfig config )
        : m_callback{ std::make_unique<DebugUtilsCallbackFunctionType>( config.m_callback_func ) }
    {
        auto create_info = makeCreateInfo( config.m_severity_flags, config.m_type_flags );
        create_info.pUserData = m_callback.get();
        m_handle = instance.createDebugUtilsMessengerEXTUnique( create_info, nullptr );
    } // DebugMessenger

    static vk::DebugUtilsMessengerCreateInfoEXT makeCreateInfo(
        vk::DebugUtilsMessageSeverityFlagsEXT severity_flags,
        vk::DebugUtilsMessageTypeFlagsEXT type_flags )
    {
        return {
            .messageSeverity = severity_flags,
            .messageType = type_flags,
            .pfnUserCallback = debugCallback,
            .pUserData = nullptr //
        };
    } // makeCreateInfo

    operator bool() const { return m_handle.get(); }
}; // DebugMessenger

} // namespace vkwrap