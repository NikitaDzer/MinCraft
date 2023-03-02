#pragma once

#include "common/utility.h"
#include "common/vulkan_include.h"

#include <range/v3/range.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view.hpp>
#include <range/v3/view/concat.hpp>

#include "vkwrap/core.h"
#include "vkwrap/debug.h"

#include <spdlog/fmt/bundled/core.h>
#include <spdlog/spdlog.h>

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
    virtual const vk::Instance& get() const& = 0;
    virtual vk::Instance& get() & = 0;

    vk::Instance* operator->() { return std::addressof( get() ); }
    const vk::Instance* operator->() const { return std::addressof( get() ); }

    virtual ~IInstance() {}
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

    const vk::Instance& get() const& override { return RawInstanceImpl::get(); }
    vk::Instance& get() & override { return RawInstanceImpl::get(); }
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
        std::function<DebugMessenger::CallbackType> callback = defaultDebugCallback,
        ExtType&& extensions = {},
        LayerType&& layers = {} )
        : RawInstanceImpl{ version, p_info, addDebugUtilsExtension( extensions ), std::forward<LayerType>( layers ) },
          DebugMessenger{ RawInstanceImpl::get(), callback }
    {
    }

    using RawInstanceImpl::get;
    using RawInstanceImpl::operator*;
    using RawInstanceImpl::operator->;

    const vk::Instance& get() const& override { return RawInstanceImpl::get(); }
    vk::Instance& get() & override { return RawInstanceImpl::get(); }
};

class GenericInstance final
{
  private:
    using HandleType = std::unique_ptr<IInstance>;

  private:
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
    }

    const vk::Instance& get() const& { return ( *m_handle ).get(); }
    vk::Instance& get() & { return ( *m_handle ).get(); }

    vk::Instance* operator->() { return std::addressof( get() ); }
    const vk::Instance* operator->() const { return std::addressof( get() ); }

    operator bool() { return static_cast<bool>( get() ); } // Type coercion to check whether handle is not empty.
};

} // namespace vkwrap