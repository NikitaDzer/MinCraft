/*
 * -- GLFW thread safety key points --
 *
 * Initialization, termination, event processing and the creation and destruction of windows, cursors
 * and OpenGL and OpenGL ES contexts are all restricted to the main thread.
 *
 * Because event processing must be performed on the main thread, all callbacks except for the error callback
 * will only be called on that thread.
 *
 * All Vulkan related functions may be called from any thread.
 *
 */

#pragma once

#include "common/glfw_include.h"
#include "common/vulkan_include.h"
#include "window/window_base.h"

#include "glfw/core.h"

#include <spdlog/fmt/bundled/format.h>
#include <spdlog/spdlog.h>

#include <functional>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace glfw::wnd
{

using WindowHandle = GLFWwindow;

struct FramebufferSizeType
{
    int width = 0;
    int height = 0;
};

using WindowResizeCallbackSignature = void(
    int width,
    int height //
);

using ResizeCallbackFunction = std::function<WindowResizeCallbackSignature>;

struct WindowConfig
{
  public:
    int width = k_default_width;
    int height = k_default_height;
    std::string title = std::string{ k_default_title };
    bool fullscreen = k_default_fullscreen;
    ResizeCallbackFunction resize_callback = defaultResizeCallback;

  private:
    static constexpr int k_default_width = 640;
    static constexpr int k_default_height = 480;
    static constexpr auto k_default_title = "Default Window Name";
    static constexpr bool k_default_fullscreen = false;

    static void defaultResizeCallback( int, int )
    {
        spdlog::warn( "No resize callback is set." );
    } // defaultResizeCallback

}; // struct WindowConfig

namespace detail
{

class WindowImpl
{
  private:
    // Call permissions: main thread.
    static void destroyWindow( WindowHandle* glfw_window );

    // Call permissions: main thread.
    static WindowHandle* createWindow(
        const WindowConfig& config,
        WindowImpl* bound_handle //
    );

    // Call permissions: any thread.
    static WindowImpl& getHandle( WindowHandle* glfw_window )
    {
        auto* ptr = reinterpret_cast<WindowImpl*>( glfwGetWindowUserPointer( glfw_window ) );
        assert( ptr );
        return *ptr;
    } // getHandle

    static void framebufferSizeCallback(
        WindowHandle* glfw_window,
        int width,
        int height //
    );

  private:
    WindowImpl( const WindowConfig& config )
        : m_handle{ createWindow( config, this ), CustomDeleter{} },
          m_resize_callback{ config.resize_callback }
    {
    }

    WindowImpl( const WindowImpl& ) = delete;
    WindowImpl( WindowImpl&& ) = delete;

    WindowImpl& operator=( const WindowImpl& ) = delete;
    WindowImpl& operator=( WindowImpl&& ) = delete;

  public:
    ~WindowImpl() = default;

  public:
    static auto create( const WindowConfig& config )
    {
        // Return a dynamically allocated object of self, so that pointer does not dangle
        return std::unique_ptr<WindowImpl>{ new WindowImpl{ config } };
    }

  public:
    WindowHandle* get() const noexcept { return m_handle.get(); }
    operator WindowHandle*() const noexcept { return get(); }

    // Call permissions: any thread.
    bool running() const { return !glfwWindowShouldClose( m_handle.get() ); }

    // Call permissions: main thread.
    FramebufferSizeType getFramebufferSize() const;

    // Call permissions: main thread.
    void setWindowed();

    // Call permissions: main thread.
    void setFullscreen();

    // Call permissions: any thread.
    ::wnd::UniqueSurface createSurface( const vk::Instance& instance ) const;

  private:
    struct CustomDeleter
    {
        void operator()( WindowHandle* ptr ) { destroyWindow( ptr ); }
    }; // CustomDeleter

  private:
    std::unique_ptr<WindowHandle, CustomDeleter> m_handle;
    std::function<WindowResizeCallbackSignature> m_resize_callback;

}; // class WindowImpl

} // namespace detail

class Window : public ::wnd::WindowBase<glfw::wnd::Window>
{
  public:
    Window( const WindowConfig& config = {} )
        : m_pimpl{ detail::WindowImpl::create( config ) }
    {
    }

  public:
    WindowHandle* get() const noexcept { return m_pimpl->get(); }
    operator WindowHandle*() const noexcept { return get(); }

    // Call permissions: any thread.
    bool running() const { return m_pimpl->running(); }

    // Call permissions: main thread.
    FramebufferSizeType getFramebufferSize() const { return m_pimpl->getFramebufferSize(); }

    // Call permissions: main thread.
    int getFramebufferWidth() const { return getFramebufferSize().width; }

    // Call permissions: main thread.
    int getFramebufferHeight() const { return getFramebufferSize().height; }

    // Call permissions: main thread.
    void setWindowed() { m_pimpl->setWindowed(); }

    // Call permissions: main thread.
    void setFullscreen() { m_pimpl->setFullscreen(); }

    // Call permissions: any thread.
    ::wnd::UniqueSurface createSurface( const vk::Instance& instance ) const
    {
        return m_pimpl->createSurface( instance );
    }

  private:
    std::unique_ptr<detail::WindowImpl> m_pimpl;
};

} // namespace glfw::wnd

namespace wnd
{

template class WindowBase<glfw::wnd::Window>;
static_assert( WindowWrapper<glfw::wnd::Window>, "GLFW window wrapper does not satisfy constaints" );

} // namespace wnd