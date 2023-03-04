#pragma once

#include "common/utils.h"
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

class DebugMessenger
{
  public:
    using CallbackType = bool(
        vk::DebugUtilsMessageSeverityFlagBitsEXT,
        vk::DebugUtilsMessageTypeFlagsEXT,
        const vk::DebugUtilsMessengerCallbackDataEXT& );

  private:
    using HandleType = vk::UniqueHandle<vk::DebugUtilsMessengerEXT, vk::DispatchLoaderDynamic>;

    std::unique_ptr<std::function<CallbackType>> m_callback; // Here unqiue ptr is needed so that if the object is
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

    DebugMessenger(
        vk::Instance instance,
        std::function<CallbackType> callback,
        vk::DebugUtilsMessageSeverityFlagsEXT severity_flags = k_default_severity_flags,
        vk::DebugUtilsMessageTypeFlagsEXT type_flags = k_default_type_flags )
        : m_callback{ std::make_unique<std::function<CallbackType>>( callback ) }
    {
        vk::DebugUtilsMessengerCreateInfoEXT create_info = {
            .messageSeverity = severity_flags,
            .messageType = type_flags,
            .pfnUserCallback = debugCallback,
            .pUserData = m_callback.get() };
        m_handle = instance.createDebugUtilsMessengerEXTUnique( create_info, nullptr );
    } // DebugMessenger

    operator bool() const { return m_handle.get(); }
}; // DebugMessenger

std::string
assembleDebugMessage(
    vk::DebugUtilsMessageTypeFlagsEXT message_types,
    const vk::DebugUtilsMessengerCallbackDataEXT& data );

bool
defaultDebugCallback(
    vk::DebugUtilsMessageSeverityFlagBitsEXT message_severity,
    vk::DebugUtilsMessageTypeFlagsEXT message_types,
    const vk::DebugUtilsMessengerCallbackDataEXT& callback_data );

} // namespace vkwrap