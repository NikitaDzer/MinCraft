#pragma once

#include "common/spdlog_inlcude.h"
#include "common/utility.h"
#include "common/vulkan_include.h"

#include <range/v3/range.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view.hpp>
#include <range/v3/view/concat.hpp>

#include <array>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

namespace vkwrap
{

class IInstance
{
  public:
    virtual vk::Instance get() const = 0;
    virtual ~IInstance() {}
};

enum class VulkanVersion : uint32_t
{
    e_version_1_0 = VK_MAKE_API_VERSION( 0, 1, 0, 0 ),
    e_version_1_1 = VK_MAKE_API_VERSION( 0, 1, 1, 0 ),
    e_version_1_2 = VK_MAKE_API_VERSION( 0, 1, 2, 0 ),
    e_version_1_3 = VK_MAKE_API_VERSION( 0, 1, 3, 0 ),
};

namespace detail
{

struct RawInstanceImpl : protected vk::UniqueInstance
{
  private:
    using BaseType = vk::UniqueInstance;

    BaseType createHandle(
        VulkanVersion version,
        const vk::ApplicationInfo* p_app_info,
        auto&& extensions,
        auto&& layers )
    {
        const auto app_info = vk::ApplicationInfo{ .apiVersion = utility::toUnderlying( version ) };
        auto info_pointer = ( p_app_info ? p_app_info : &app_info );

        const auto raw_extensions = utility::convertToCStrVector( extensions );
        const auto raw_layers = utility::convertToCStrVector( layers );

        const auto create_info = vk::InstanceCreateInfo{
            .pApplicationInfo = info_pointer,
            .enabledLayerCount = static_cast<uint32_t>( raw_layers.size() ),
            .ppEnabledLayerNames = raw_layers.data(),
            .enabledExtensionCount = static_cast<uint32_t>( raw_extensions.size() ),
            .ppEnabledExtensionNames = raw_extensions.data(),
        };

        return vk::createInstanceUnique( create_info );
    }

  public:
    template <typename ExtType = ranges::empty_view<std::string>, typename LayerType = ranges::empty_view<std::string>>
    RawInstanceImpl(
        VulkanVersion version,
        const vk::ApplicationInfo* p_info = nullptr,
        ExtType&& extensions = {},
        LayerType&& layers = {} )
        : BaseType{
              createHandle( version, p_info, std::forward<ExtType>( extensions ), std::forward<LayerType>( layers ) ) }
    {
    }

    using BaseType::get;
    using BaseType::operator*;
    using BaseType::operator->;
};

} // namespace detail

class Instance : public IInstance, private detail::RawInstanceImpl
{
  public:
    using RawInstanceImpl::RawInstanceImpl;

    using RawInstanceImpl::get;
    using RawInstanceImpl::operator*;
    using RawInstanceImpl::operator->;

    vk::Instance get() const override { return RawInstanceImpl::get(); }
};

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

    HandleType m_handle;
    vk::DispatchLoaderDynamic m_loader;
    std::unique_ptr<std::function<CallbackType>> m_callback; // Here unqiue ptr is needed so that if the object is
                                                             // moved from, then user_data pointer does not change.

  private:
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
        VkDebugUtilsMessageTypeFlagsEXT message_types,
        const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
        void* user_data )
    {
        auto* ptr = static_cast<std::function<CallbackType>*>( user_data );

        // NOTE[Sergei]: I'm not sure if callback_data ptr can be nullptr. Look here
        // https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/PFN_vkDebugUtilsMessengerCallbackEXT.html
        assert( callback_data );
        auto severity = static_cast<vk::DebugUtilsMessageSeverityFlagBitsEXT>( message_severity );
        auto types = static_cast<vk::DebugUtilsMessageTypeFlagsEXT>( message_types );
        auto data = *reinterpret_cast<const vk::DebugUtilsMessengerCallbackDataEXT*>( callback_data );
        auto result = ( *ptr )( severity, types, data );

        return ( result ? VK_TRUE : VK_FALSE );
    }

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

        m_loader = { instance, vkGetInstanceProcAddr };
        m_handle = instance.createDebugUtilsMessengerEXTUnique( create_info, nullptr, m_loader );
    }
};

inline std::string
assembleDebugMessage(
    vk::DebugUtilsMessageTypeFlagsEXT message_types,
    const vk::DebugUtilsMessengerCallbackDataEXT& data )
{
    std::stringstream ss;

    std::span<const vk::DebugUtilsLabelEXT> queues = { data.pQueueLabels, data.queueLabelCount },
                                            cmdbufs = { data.pCmdBufLabels, data.cmdBufLabelCount };
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

    ss << "Message [id_name = <" << data.pMessageIdName << ">, id_num = " << data.messageIdNumber
       << ", types = " << vk::to_string( message_types ) << "]: " << trimLeadingTrailingSpaces( data.pMessage ) << "\n";

    if ( !queues.empty() )
        ss << " -- Associated Queues: --\n";
    for ( uint32_t i = 0; const auto& v : queues )
    {
        ss << "[" << i++ << "]. name = <" << v.pLabelName << ">\n";
    }

    if ( !cmdbufs.empty() )
        ss << " -- Associated Command Buffers: --\n";
    for ( uint32_t i = 0; const auto& v : cmdbufs )
    {
        ss << "[" << i++ << "]. name = <" << v.pLabelName << ">\n";
    }

    if ( !objects.empty() )
        ss << " -- Associated Vulkan Objects: --\n";
    for ( uint32_t i = 0; const auto& v : objects )
    {
        ss << "[" << i++ << "]. type = <" << vk::to_string( v.objectType ) << ">, handle = " << v.objectHandle;
        if ( v.pObjectName )
            ss << ", name = <" << v.pObjectName << ">";
        ss << "\n";
    }

    return ss.str();
};

inline bool
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
};

class DebuggedInstance : public IInstance, private detail::RawInstanceImpl, private DebugMessenger
{
  private:
    static constexpr auto k_debug_utils_ext_name =
        std::to_array<std::string_view>( { VK_EXT_DEBUG_UTILS_EXTENSION_NAME } );

    static auto addDebugUtilsExtension( auto&& extensions )
    {
        return ranges::views::concat( // Overkill
                   ranges::views::transform( extensions, []( auto a ) { return std::string_view{ a }; } ),
                   k_debug_utils_ext_name ) |
            ranges::views::unique | ranges::to<std::vector<std::string>>;
    }

  public:
    template <typename ExtType = ranges::empty_view<std::string>, typename LayerType = ranges::empty_view<std::string>>
    DebuggedInstance(
        VulkanVersion version,
        const vk::ApplicationInfo* p_info = nullptr,
        DebugMessenger::CallbackType callback = defaultDebugCallback,
        ExtType&& extensions = {},
        LayerType&& layers = {} )
        : RawInstanceImpl{ version, p_info, addDebugUtilsExtension( extensions ), std::forward<LayerType>( layers ) },
          DebugMessenger{ RawInstanceImpl::get(), callback }
    {
    }

    using RawInstanceImpl::get;
    using RawInstanceImpl::operator*;
    using RawInstanceImpl::operator->;

    vk::Instance get() const override { return RawInstanceImpl::get(); }
};

} // namespace vkwrap