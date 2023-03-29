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
#include "common/window_base.h"
#include "error.h"

#include <spdlog/fmt/bundled/format.h>
#include <spdlog/spdlog.h>

#include <functional>
#include <iostream>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace wnd::glfw
{

class Window;
class WindowManager;

struct WindowConfig;

using WindowType = GLFWwindow;
using FramebufferSizeType = std::pair<int, int>;

using WindowResizeCallbackSignature = void(
    Window* window,
    int width,
    int height //
);

struct WindowAPIversion
{
    int major = 0;
    int minor = 0;

    auto operator<=>( const WindowAPIversion& ) const = default;
    std::string to_string() const { return fmt::format( "{}.{}", major, minor ); }
}; // WindowAPIversion

struct WindowConfig
{
  public:
    int width = k_default_width;
    int height = k_default_height;
    std::string title = std::string{ k_default_title };
    bool fullscreen = k_default_fullscreen;

    using ResizeCallbackFunction = std::function<WindowResizeCallbackSignature>;
    ResizeCallbackFunction resize_callback = defaultResizeCallback;

  private:
    static constexpr int k_default_width = 640;
    static constexpr int k_default_height = 480;
    static constexpr std::string_view k_default_title = "MinCraft";
    static constexpr bool k_default_fullscreen = false;

    static void defaultResizeCallback(
        Window* window,
        int width,
        int height //
    )
    {
        spdlog::warn( "No resize callback is set." );
    } // defaultResizeCallback
};    // struct WindowConfig

class Window : public WindowBase<Window>
{
  private:
    // Call permissions: main thread.
    static void destroyWindow( WindowType* glfwWindow );

    struct CustomDeleter
    {
        void operator()( WindowType* ptr ) { destroyWindow( ptr ); }
    }; // CustomDeleter

  private:
    using HandleType = std::unique_ptr<WindowType, CustomDeleter>;

  private:
    HandleType m_handle;
    std::function<WindowResizeCallbackSignature> m_resize_callback;

    // Call permissions: main thread.
    static WindowType* createWindow(
        const WindowConfig& config,
        Window* bound_handle //
    );

    // Call permissions: any thread.
    static Window* getHandle( WindowType* glfwWindow )
    {
        return reinterpret_cast<Window*>( glfwGetWindowUserPointer( glfwWindow ) );
    } // getHandle

    static void framebufferSizeCallback(
        WindowType* glfwWindow,
        int width,
        int height //
    );

  public:
    Window( const WindowConfig& config = {} )
        : m_handle{ createWindow( config, this ), CustomDeleter{} },
          m_resize_callback{ config.resize_callback }
    {
    }

    WindowType* get() const noexcept { return m_handle.get(); }

    // Call permissions: any thread.
    bool running() const { return !glfwWindowShouldClose( m_handle.get() ); }

    // Call permissions: main thread.
    FramebufferSizeType getFramebufferSize() const;

    // Call permissions: main thread.
    int getFramebufferWidth() const { return getFramebufferSize().first; }

    // Call permissions: main thread.
    int getFramebufferHeight() const { return getFramebufferSize().second; }

    // Call permissions: main thread.
    void setWindowed() const;

    // Call permissions: main thread.
    void setFullscreen() const;

    // Call permissions: any thread.
    UniqueSurface createSurface( const vk::Instance& instance ) const;

}; // class Window

class WindowManager
{

  private:
    using ErrorCallbackSignature = void(
        int error_code,
        const char* description //
    );

    WindowManager( ErrorCallbackSignature* error_callback );
    ~WindowManager();

    static bool isSuitableVersion();

  public:
    static constexpr WindowAPIversion k_api_min_version{ .major = 3, .minor = 3 };

    // Call permissions: main thread.
    static void initialize();

    // Call permission: any thread, after WindowManager::initialize.
    static std::span<const char*> getRequiredExtensions();

    static std::string getMinVersionString();

}; // class WindowManager

} // namespace wnd::glfw

namespace wnd
{

template class WindowBase<glfw::Window>;
static_assert( WindowWrapper<glfw::Window>, "GLFW window wrapper does not satisfy constaints" );

}; // namespace wnd