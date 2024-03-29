#include "vkwrap/debug.h"
#include "common/vulkan_include.h"

#include <spdlog/fmt/bundled/core.h>
#include <spdlog/spdlog.h>

#include <exception>
#include <functional>
#include <iterator>
#include <span>
#include <stdexcept>
#include <string>

namespace vkwrap
{

static std::string
trimLeadingTrailingSpaces( const std::string& input )
{
    if ( input.empty() )
    {
        return {};
    }

    auto pos_first = input.find_first_not_of( " \t\n" );
    auto pos_last = input.find_last_not_of( " \t\n" );
    return input.substr( pos_first != std::string::npos ? pos_first : 0, ( pos_last - pos_first ) + 1 );
} // trimLeadingTrailingSpaces

std::string
assembleDebugMessage(
    vk::DebugUtilsMessageTypeFlagsEXT message_types,
    const vk::DebugUtilsMessengerCallbackDataEXT& data )
{
    std::string output;
    auto oit = std::back_inserter( output );

    const auto queues = std::span<const vk::DebugUtilsLabelEXT>{ data.pQueueLabels, data.queueLabelCount };
    const auto cmdbufs = std::span<const vk::DebugUtilsLabelEXT>{ data.pCmdBufLabels, data.cmdBufLabelCount };
    const auto objects = std::span<const vk::DebugUtilsObjectNameInfoEXT>{ data.pObjects, data.objectCount };

    fmt::format_to(
        oit,
        "Message [id_name = <{}>, id_num = {}, types = {}]: {}\n",
        data.pMessageIdName,
        data.messageIdNumber,
        vk::to_string( message_types ),
        trimLeadingTrailingSpaces( data.pMessage ) //
    );

    if ( !queues.empty() )
    {
        fmt::format_to( oit, " -- Associated Queues: --\n" );
    }

    for ( uint32_t i = 0; const auto& v : queues )
    {
        fmt::format_to( oit, "[{}]. name = <{}>\n", i++, v.pLabelName );
    }

    if ( !cmdbufs.empty() )
    {
        fmt::format_to( oit, " -- Associated Command Buffers: --\n" );
    }

    for ( uint32_t i = 0; const auto& v : cmdbufs )
    {
        fmt::format_to( oit, "[{}]. name = <{}>\n", i++, v.pLabelName );
    }

    if ( !objects.empty() )
    {
        fmt::format_to( oit, " -- Associated Vulkan Objects: --\n" );
    }

    for ( uint32_t i = 0; const auto& v : objects )
    {
        fmt::format_to( oit, "[{}]. type = <{}>, handle = {:#x}", i++, vk::to_string( v.objectType ), v.objectHandle );
        if ( v.pObjectName )
        {
            fmt::format_to( oit, ", name = <{}>", v.pObjectName );
        }
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

    auto msg_str = assembleDebugMessage( message_types, callback_data );
    switch ( message_severity )
    {
    case MsgSev::eInfo:
        spdlog::info( msg_str );
        break;
    case MsgSev::eWarning:
        spdlog::warn( msg_str );
        break;
    case MsgSev::eVerbose:
        spdlog::debug( msg_str );
        break;
    case MsgSev::eError:
        spdlog::error( msg_str );
        break;
    default:
        assert( 0 && "[Debug]: Broken message severity enum" );
        std::terminate();
        break;
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
    const auto* func_ptr = static_cast<DebugUtilsCallbackFunctionType*>( user_data );

    // NOTE[Sergei]: I'm not sure if callback_data ptr can be nullptr. Look here
    // https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/PFN_vkDebugUtilsMessengerCallbackEXT.html
    assert( callback_data && "[Debug]: Broken custom user data pointer, can't call user callback " );
    const auto severity = static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>( message_severity );
    const auto types = static_cast<vk::DebugUtilsMessageTypeFlagsEXT>( message_types );
    const auto data = *reinterpret_cast<const vk::DebugUtilsMessengerCallbackDataEXT*>( callback_data );

    bool result = true;
    // When this function is used as a callback for instance creation debugging, user_data can't be used to pass
    // custom callbacks. Thus, it is necessary to use the fallback callback.
    if ( func_ptr )
    {
        result = ( *func_ptr )( severity, types, data );
    } else
    {
        result = defaultDebugCallback( severity, types, data );
    }

    return ( result ? VK_TRUE : VK_FALSE );
} // DebugMessenger::debugCallback

} // namespace vkwrap