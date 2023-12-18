#pragma once

#include "common/vulkan_include.h"

#include "utils/algorithm.h"
#include "utils/misc.h"
#include "utils/range.h"

#include <range/v3/iterator.hpp>
#include <range/v3/range/concepts.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view.hpp>
#include <range/v3/view/concat.hpp>

#include "vkwrap/core.h"
#include "vkwrap/debug.h"
#include "vkwrap/error.h"

#include <spdlog/spdlog.h>

#include <array>
#include <cstdint>
#include <functional>
#include <initializer_list>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace vkwrap
{

// Interface, base class for Instance wrapper implementations. Defines virtual accessors and implements operator->
// common to concrete wrappers.
class IInstance
{
  public:
    virtual const vk::Instance& get() const& = 0;
    virtual vk::Instance& get() & = 0;

    vk::Instance* operator->() { return std::addressof( get() ); }
    const vk::Instance* operator->() const { return std::addressof( get() ); }

    virtual ~IInstance() {}
}; // IInstance

namespace detail
{

struct InstanceImpl : protected vk::UniqueInstance
{
  private:
    using BaseType = vk::UniqueInstance;

  public:
    using OptionalDebugCreateInfo = std::optional<vk::DebugUtilsMessengerCreateInfoEXT>;

    [[nodiscard]] static SupportsResult supportsExtensions( ranges::range auto&& find )
    {
        const auto supported_extensions = vk::enumerateInstanceExtensionProperties();
        const auto missing_extensions = utils::findAllMissing( supported_extensions, find, []( auto&& ext ) {
            // cast to const char* because decltype( extensionName ) == ArrayWrapper1D<char, 256>
            // that lead to creation of string_view with size() == 256. Therefore the comparison between
            // strings will be incorrect ( because comparison relies on string size as well )
            return std::string_view{ reinterpret_cast<const char*>( &ext.extensionName ) };
        } );

        return SupportsResult{ missing_extensions.empty(), missing_extensions | ranges::to<StringVector> };
    } // supportsExtensions

    [[nodiscard]] static SupportsResult supportsLayers( ranges::range auto&& find )
    {
        const auto supported_layers = vk::enumerateInstanceLayerProperties();
        const auto missing_layers = utils::findAllMissing( supported_layers, find, []( auto&& layer ) {
            return std::string_view{ reinterpret_cast<const char*>( &layer.layerName ) };
        } );

        return SupportsResult{ missing_layers.empty(), missing_layers | ranges::to<StringVector> };

    } // supportsLayers

  private:
    BaseType createHandle(
        VulkanVersion version,
        ranges::range auto&& extensions,
        ranges::range auto&& layers,
        OptionalDebugCreateInfo debug_info )
    {
        validateExtensionsLayers( extensions, layers );
        const auto app_info = vk::ApplicationInfo{ .apiVersion = utils::toUnderlying( version ) };
        const auto raw_extensions = utils::convertToCStrVector( extensions );
        const auto raw_layers = utils::convertToCStrVector( layers );
        const auto debug_info_ptr = debug_info ? &debug_info.value() : nullptr;

        const auto create_info = vk::InstanceCreateInfo{
            .pNext = static_cast<const void*>( debug_info_ptr ),
            .pApplicationInfo = &app_info,
            .enabledLayerCount = static_cast<uint32_t>( raw_layers.size() ),
            .ppEnabledLayerNames = raw_layers.data(),
            .enabledExtensionCount = static_cast<uint32_t>( raw_extensions.size() ),
            .ppEnabledExtensionNames = raw_extensions.data(),
        };

        auto instance = vk::createInstanceUnique( create_info );
        VULKAN_HPP_DEFAULT_DISPATCHER.init( *instance );
        return instance;
    } // createHandle

    // Check that the instance actually supports requested extensions/layers and throw an informative error when it
    // doesn't.
    void validateExtensionsLayers( ranges::range auto&& extensions, ranges::range auto&& layers )
    {
        auto [ ext_ok, missing_ext ] = supportsExtensions( extensions );
        auto [ layers_ok, missing_layers ] = supportsLayers( layers );

        if ( !ext_ok || !layers_ok )
        {
            auto tagged_ext = ranges::views::transform( missing_ext, []( auto&& elem ) {
                return UnsupportedEntry{ UnsupportedTag::e_unsupported_extension, elem };
            } );

            auto tagged_layers = ranges::views::transform( missing_layers, []( auto&& elem ) {
                return UnsupportedEntry{ UnsupportedTag::e_unsupported_layer, elem };
            } );

            auto missing = ranges::views::concat( tagged_ext, tagged_layers ) | ranges::to_vector;
            throw UnsupportedError{ "Instance does not support all required layers (and/or) extensions", missing };
        }
    } // validateExtensionsLayers

  public:
    template <
        ranges::range Ext = ranges::empty_view<const char*>,
        ranges::range Layer = ranges::empty_view<const char*>>
    InstanceImpl( VulkanVersion version, Ext&& ext = {}, Layer&& layers = {}, OptionalDebugCreateInfo p_debug = {} )
        : BaseType{ createHandle( version, std::forward<Ext>( ext ), std::forward<Layer>( layers ), p_debug ) }
    {
    }

    using BaseType::get;
    using BaseType::operator*;
    using BaseType::operator->;
    using BaseType::operator bool;
}; // InstanceImpl

} // namespace detail

// Instance wrapper with simpler constructor.
class Instance : public IInstance, private detail::InstanceImpl
{
  public:
    template <
        ranges::range ExtType = ranges::empty_view<const char*>,
        ranges::range LayerType = ranges::empty_view<const char*>>
    Instance( VulkanVersion version, ExtType&& extensions = {}, LayerType&& layers = {} )
        : InstanceImpl{ version, std::forward<ExtType>( extensions ), std::forward<LayerType>( layers ) }
    {
    }

    using InstanceImpl::get;
    using InstanceImpl::operator*;
    using InstanceImpl::operator->;
    using InstanceImpl::operator bool;

    const vk::Instance& get() const& override { return InstanceImpl::get(); }
    vk::Instance& get() & override { return InstanceImpl::get(); }

    using InstanceImpl::supportsExtensions;
    using InstanceImpl::supportsLayers;
}; // Instance

// Instance wrapper that provides a VkDebugUtilsExt debug messenger with configurable callback.
class DebuggedInstance : public IInstance, private detail::InstanceImpl, private DebugMessenger
{
  private:
    static constexpr auto k_debug_utils_ext_name =
        std::to_array<std::string_view>( { VK_EXT_DEBUG_UTILS_EXTENSION_NAME } );

    static auto addDebugUtilsExtension( ranges::range auto&& extensions )
    {
        return ranges::views::concat( // Overkill
                   ranges::views::transform( extensions, []( auto&& a ) { return std::string_view{ a }; } ),
                   k_debug_utils_ext_name ) |
            ranges::views::unique | ranges::to<StringVector>;
    } // addDebugUtilsExtension

    static vk::DebugUtilsMessengerCreateInfoEXT makeDebugCreateInfo( DebugMessengerConfig debug_config )
    {
        return DebugMessenger::makeCreateInfo( debug_config.m_severity_flags, debug_config.m_type_flags );
    } // makeDebugCreateInfo

  public:
    template <
        ranges::range ExtType = ranges::empty_view<const char*>,
        ranges::range LayerType = ranges::empty_view<const char*>>
    DebuggedInstance(
        VulkanVersion version,
        DebugMessengerConfig debug_config = {},
        ExtType&& extensions = {},
        LayerType&& layers = {} )
        : InstanceImpl{ version, addDebugUtilsExtension( std::forward<ExtType>( extensions ) ), std::forward<LayerType>( layers ), makeDebugCreateInfo( debug_config ) },
          DebugMessenger{ InstanceImpl::get(), debug_config }
    {
    }

    using InstanceImpl::get;
    using InstanceImpl::operator*;
    using InstanceImpl::operator->;

    const vk::Instance& get() const& override { return InstanceImpl::get(); }
    vk::Instance& get() & override { return InstanceImpl::get(); }

    using InstanceImpl::supportsExtensions;
    using InstanceImpl::supportsLayers;
    using InstanceImpl::operator bool;
}; // DebuggedInstance

class GenericInstance final
{
  private:
    using HandleType = std::unique_ptr<IInstance>;

  private:
    // This wrapper is basically a PImpl that can either be a regular Instance or DebuggedInstance.
    HandleType m_handle;

    // Note that this constructor is private and other constructors never allow a nullptr std::unique_ptr to be created.
    // Thus this class holds an invariant of (m_handle.get() != nullptr) and it's safe to dereference it anywhere.
    GenericInstance( HandleType handle ) { m_handle = std::move( handle ); }

  public:
    GenericInstance( Instance handle ) { m_handle = std::make_unique<Instance>( std::move( handle ) ); }
    GenericInstance( DebuggedInstance handle ) { m_handle = std::make_unique<DebuggedInstance>( std::move( handle ) ); }

    template <typename T, typename... Ts> static GenericInstance make( Ts&&... args )
    {
        return GenericInstance{ std::make_unique<T>( std::forward<Ts>( args )... ) };
    } // make

    const vk::Instance& get() const& { return ( *m_handle ).get(); }
    vk::Instance& get() & { return ( *m_handle ).get(); }

    vk::Instance* operator->() { return std::addressof( get() ); }
    const vk::Instance* operator->() const { return std::addressof( get() ); }

    operator bool() { return static_cast<bool>( get() ); } // Type coercion to check whether handle is not empty.
    operator vk::Instance() { return get(); }
}; // GenericInstance

class InstanceBuilder
{
  private:
    VulkanVersion m_version = VulkanVersion::e_version_1_0;
    bool m_with_debug = false;

    StringVector m_extensions;
    StringVector m_layers;

    DebugMessengerConfig m_debug_config;

  private:
    static constexpr auto k_validation_layer_name = "VK_LAYER_KHRONOS_validation";

  private:
    DebuggedInstance makeDebugInstance() const
    {
        return DebuggedInstance{ m_version, m_debug_config, m_extensions, m_layers };
    } // makeDebugInstance

    Instance makeInstance() const { return Instance{ m_version, m_extensions, m_layers }; }

  public:
    InstanceBuilder() = default;

    GenericInstance make() const
    {
        if ( m_with_debug )
        {
            return makeDebugInstance();
        }
        return makeInstance();
    } // make

    InstanceBuilder& withDebugMessenger() &
    {
        m_with_debug = true;
        return *this;
    } // withDebugMessenger

    InstanceBuilder& withValidationLayers() &
    {
        m_layers.push_back( k_validation_layer_name );
        return *this;
    } // withValidationLayers

    InstanceBuilder& withCallback(
        DebugUtilsCallbackFunctionType func,
        vk::DebugUtilsMessageSeverityFlagsEXT severity_flags = k_default_severity_flags,
        vk::DebugUtilsMessageTypeFlagsEXT type_flags = k_default_type_flags ) &
    {
        m_debug_config = DebugMessengerConfig{ func, severity_flags, type_flags };
        return *this;
    } // withCallback

    template <ranges::range Range> InstanceBuilder& withExtensions( Range&& extensions ) &
    {
        ranges::copy( std::forward<Range>( extensions ), ranges::back_inserter( m_extensions ) );
        return *this;
    } // withExtensions

    template <ranges::range Range> InstanceBuilder& withLayers( Range&& layers ) &
    {
        ranges::copy( std::forward<Range>( layers ), ranges::back_inserter( m_layers ) );
        return *this;
    } // withLayers

    InstanceBuilder& withVersion( VulkanVersion version ) &
    {
        m_version = version;
        return *this;
    } // withVersion
};    // InstanceBuilder

} // namespace vkwrap
