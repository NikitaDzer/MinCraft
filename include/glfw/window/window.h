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

#include <functional>
#include <iostream>
#include <memory>
#include <span>
#include <spdlog/spdlog.h>
#include <string>

#include "common/glfw_include.h"
#include "common/vulkan_include.h"
#include "error.h"

namespace wnd
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
    int major;
    int minor;

    auto operator<=>( const WindowAPIversion& ) const = default;
}; // WindowAPIversion

struct WindowConfig
{
    const int width = k_default_width;
    const int height = k_default_height;
    const char* title = k_default_title;
    const bool fullscreen = k_default_fullscreen;

    std::function<WindowResizeCallbackSignature> resize_callback = defaultResizeCallback;

    static constexpr int k_default_width = 640;
    static constexpr int k_default_height = 480;
    static constexpr char k_default_title[] = "MinCraft";
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

class Window
{

  private:
    using DeleteFunctionSignature = void( WindowType* );
    using HandleType = std::unique_ptr<WindowType, DeleteFunctionSignature*>;

  private:
    HandleType m_handle;
    std::function<WindowResizeCallbackSignature> m_resize_callback;

    // Call permissions: main thread.
    static WindowType* createWindow(
        const WindowConfig& config,
        Window* bound_handle //
    );

    // Call permissions: main thread.
    static void destroyWindow( WindowType* glfwWindow );

    // Call permissions: any thread.
    static Window* getHandle( WindowType* glfwWindow )
    {
        return reinterpret_cast<Window*>( glfwGetWindowUserPointer( glfwWindow ) );
    }

    static void framebufferSizeCallback(
        WindowType* glfwWindow,
        int width,
        int height //
    );

  public:
    Window( const WindowConfig& config = {} )
        : m_handle{ createWindow( config, this ), destroyWindow },
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
    static vk::SurfaceKHR createWindowSurface(
        vk::Instance instance,
        Window& window //
    );

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

    static const std::string getMinVersionString();

}; // class WindowManager

} // namespace wnd
