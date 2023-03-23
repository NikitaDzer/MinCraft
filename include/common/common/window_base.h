#pragma once

#include "common/vulkan_include.h"

#include <concepts>

namespace wnd
{

template <typename Derived> class WindowBase
{
  protected:
    WindowBase() = default;

  public:
    Derived& impl() { return static_cast<Derived&>( *this ); }
    const Derived& impl() const { return static_cast<const Derived&>( *this ); }

  public:
    vk::UniqueSurfaceKHR createSurface( vk::Instance instance ) const { return impl().createSurface( instance ); }
};

// clang-format off
template <typename T>
concept WindowWrapper = requires ( T derived, vk::Instance instance ) 
{
    requires std::derived_from<T, WindowBase<T>>;
    { derived.createSurface( instance ) } -> std::convertible_to<vk::UniqueSurfaceKHR>;
};
// clang-format on

} // namespace wnd