#include "vkwrap/debug.h"
#include "common/vulkan_include.h"

#include <spdlog/fmt/bundled/core.h>
#include <spdlog/spdlog.h>

#include <functional>
#include <iterator>
#include <span>
#include <string>

namespace vkwrap
{

std::string
assembleDebugMessage(
    vk::DebugUtilsMessageTypeFlagsEXT message_types,
    const vk::DebugUtilsMessengerCallbackDataEXT& data )
{
    std::string output;
    auto oit = std::back_inserter( output );

    std::span<const vk::DebugUtilsLabelEXT> queues = { data.pQueueLabels, data.queueLabelCount };
    std::span<const vk::DebugUtilsLabelEXT> cmdbufs = { data.pCmdBufLabels, data.cmdBufLabelCount };

    std::span<const vk::DebugUtilsObjectNameInfoEXT> objects = { data.pObjects, data.objectCount };

    const auto trimLeadingTrailingSpaces = []( std::string input ) -> std::string {
        if ( input.empty() )
        {
            return {};
        }

        auto pos_first = input.find_first_not_of( " \t\n" );
        auto pos_last = input.find_last_not_of( " \t\n" );
        return input.substr( pos_first != std::string::npos ? pos_first : 0, ( pos_last - pos_first ) + 1 );
    };

    fmt::format_to(
        oit,
        "Message [id_name = <{}>, id_num = {}, types = {}]: {}\n",
        data.pMessageIdName,
        data.messageIdNumber,
        vk::to_string( message_types ),
        trimLeadingTrailingSpaces( data.pMessage ) //
    );

    if ( !queues.empty() )
        fmt::format_to( oit, " -- Associated Queues: --\n" );
    for ( uint32_t i = 0; const auto& v : queues )
    {
        fmt::format_to( oit, "[{}]. name = <{}>\n", i++, v.pLabelName );
    }

    if ( !cmdbufs.empty() )
        fmt::format_to( oit, " -- Associated Command Buffers: --\n" );
    for ( uint32_t i = 0; const auto& v : cmdbufs )
    {
        fmt::format_to( oit, "[{}]. name = <{}>\n", i++, v.pLabelName );
    }

    if ( !objects.empty() )
        fmt::format_to( oit, " -- Associated Vulkan Objects: --\n" );
    for ( uint32_t i = 0; const auto& v : objects )
    {
        fmt::format_to( oit, "[{}]. type = <{}>, handle = {}", i++, vk::to_string( v.objectType ), v.objectHandle );
        if ( v.pObjectName )
            fmt::format_to( oit, ", name = <{}>", v.pObjectName );
        fmt::format_to( oit, "\n" );
    }

    return output;
}; // assembleDebugMessage

bool
defaultDebugCallback(
    vk::DebugUtilsMessageSeverityFlagBitsEXT message_severity,
    vk::DebugUtilsMessageTypeFlagsEXT message_types,
    const vk::DebugUtilsMessengerCallbackDataEXT& callback_data )
{
    using msg_sev = vk::DebugUtilsMessageSeverityFlagBitsEXT;

    auto msg_str = assembleDebugMessage( message_types, callback_data );
    if ( message_severity == msg_sev::eInfo )
    {
        spdlog::info( msg_str );
    } else if ( message_severity == msg_sev::eWarning )
    {
        spdlog::warn( msg_str );
    } else if ( message_severity == msg_sev::eVerbose )
    {
        spdlog::debug( msg_str );
    } else if ( message_severity == msg_sev::eError )
    {
        spdlog::error( msg_str );
    } else
    {
        assert( 0 && "[Debug]: Broken message severity enum" );
    }

    return false;
}; // defaultDebugCallback

VKAPI_ATTR VkBool32 VKAPI_CALL
DebugMessenger::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_types,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data )
{
    auto* func_ptr = static_cast<std::function<CallbackType>*>( user_data );

    // NOTE[Sergei]: I'm not sure if callback_data ptr can be nullptr. Look here
    // https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/PFN_vkDebugUtilsMessengerCallbackEXT.html
    assert( callback_data && "[Debug]: Broken custom user data pointer, can't call user callback " );
    const auto severity = static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>( message_severity );
    const auto types = static_cast<vk::DebugUtilsMessageTypeFlagsEXT>( message_types );
    const auto data = *reinterpret_cast<const vk::DebugUtilsMessengerCallbackDataEXT*>( callback_data );
    const auto result = ( *func_ptr )( severity, types, data );

    return ( result ? VK_TRUE : VK_FALSE );
} // DebugMessenger::debugCallback

} // namespace vkwrap