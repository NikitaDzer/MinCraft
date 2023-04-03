#include <spdlog/spdlog.h>

#include "utils/misc.h"

#include "common/glfw_include.h"
#include "window/error.h"
#include "window/window.h"

#include <range/v3/view/iota.hpp>
#include <range/v3/view/take.hpp>

#include <bit>
#include <functional>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>

namespace wnd::glfw
{

namespace
{

void
setWindowHints()
{
    glfwDefaultWindowHints();

    // clang-format off
    glfwWindowHint( GLFW_RESIZABLE,               GLFW_TRUE );
    glfwWindowHint( GLFW_VISIBLE,                 GLFW_TRUE );
    glfwWindowHint( GLFW_FOCUSED,                 GLFW_TRUE );
    glfwWindowHint( GLFW_FLOATING,                GLFW_FALSE );
    glfwWindowHint( GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_FALSE );
    glfwWindowHint( GLFW_FOCUS_ON_SHOW,           GLFW_TRUE );
    glfwWindowHint( GLFW_DOUBLEBUFFER,            GLFW_TRUE );
    glfwWindowHint( GLFW_REFRESH_RATE,            GLFW_DONT_CARE );
    glfwWindowHint( GLFW_CLIENT_API,              GLFW_NO_API );
    // clang-format on
} // setWindowHints

void
logGLFWaction(
    const std::string& action,
    const void* handle = nullptr,
    const std::vector<std::string>& additional = {} //
)
{
    std::string output;
    auto oit = std::back_inserter( output );

    if ( handle )
    {
        fmt::format_to( oit, "Message (GLFW) [handle = {}]: {}", fmt::ptr( handle ), action );
    } else
    {
        fmt::format_to( oit, "Message (GLFW): {}", action );
    }

    if ( !additional.empty() )
    {
        fmt::format_to( oit, "\n -- Additional info --" );
        for ( auto i : ranges::views::iota( 0 ) | ranges::views::take( additional.size() ) )
        {
            fmt::format_to( oit, "\n[{}]. {}", i, additional[ i ] );
        }
    }

    fmt::format_to( oit, "\n" );
    spdlog::info( output );
} // logGLFWaction

void
errorCallback(
    int error_code,
    const char* description //
)
{
    throw Error{ error_code, description };
} // errorCallback

} // namespace

WindowType*
Window::createWindow(
    const WindowConfig& config,
    Window* bound_handle //
)
{
    setWindowHints();

    WindowType* glfwWindow = glfwCreateWindow(
        config.width,
        config.height,
        config.title.c_str(),
        config.fullscreen ? glfwGetPrimaryMonitor() : nullptr,
        nullptr );

    glfwSetWindowUserPointer( glfwWindow, bound_handle );
    glfwSetFramebufferSizeCallback( glfwWindow, framebufferSizeCallback );

    logGLFWaction(
        "Create Window",
        bound_handle //
    );

    return glfwWindow;
} // createWindow

void
Window::destroyWindow( WindowType* glfwWindow )
{
    Window* window = getHandle( glfwWindow );

    glfwDestroyWindow( glfwWindow );

    logGLFWaction(
        "Destroy Window",
        window //
    );
} // destroyWindow

void
Window::framebufferSizeCallback(
    WindowType* glfwWindow,
    int width,
    int height //
)
{
    Window* window = getHandle( glfwWindow );

    logGLFWaction(
        "Framebuffer resize",
        window,
        { fmt::format( "Width = {}", width ), fmt::format( "Height = ", height ) } //
    );

    window->m_resize_callback( window, width, height );
} // framebufferSizeCallback

FramebufferSizeType
Window::getFramebufferSize() const
{
    FramebufferSizeType size;
    glfwGetFramebufferSize( m_handle.get(), &size.width, &size.height );
    return size;
} // getFramebufferSize

void
Window::setWindowed() const
{
    glfwSetWindowMonitor(
        m_handle.get(),
        nullptr,
        0,
        0,
        getFramebufferWidth(),
        getFramebufferHeight(),
        GLFW_DONT_CARE );

    logGLFWaction(
        "Set windowed mode",
        this //
    );
} // setWindowed

void
Window::setFullscreen() const
{
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();

    glfwSetWindowMonitor(
        m_handle.get(),
        monitor,
        GLFW_DONT_CARE,
        GLFW_DONT_CARE,
        getFramebufferWidth(),
        getFramebufferHeight(),
        GLFW_DONT_CARE );

    logGLFWaction( "Set fullscreen mode", this );
} // setFullscreen

UniqueSurface
Window::createSurface( const vk::Instance& instance ) const
{
    assert( instance );

    VkSurfaceKHR surface;
    glfwCreateWindowSurface(
        static_cast<VkInstance>( instance ),
        m_handle.get(),
        nullptr,
        &surface //
    );

    return vk::UniqueSurfaceKHR{
        surface,
        instance }; // Very crucial to pass instance for the correct deleter with dynamic dispatch to work. This is the
                    // same reason why all files need to be compiled together.
} // createWindowSurface

WindowManager::WindowManager( ErrorCallbackSignature* error_callback )
{
    glfwSetErrorCallback( error_callback );
    glfwInit();
    logGLFWaction( "Initialize library" );
} // WindowManager

WindowManager::~WindowManager() noexcept( false )
{
    glfwTerminate();
    logGLFWaction( "Terminate library" );
} // ~WindowManager

bool
WindowManager::isSuitableVersion()
{
    WindowAPIversion version;
    glfwGetVersion( &version.major, &version.minor, nullptr );
    return version >= WindowManager::k_api_min_version;
} // isSuitableVersion

void
WindowManager::initialize()
{
    static WindowManager instance{ errorCallback };
    if ( isSuitableVersion() )
    {
        return;
    }

    throw Error{
        Error::k_user_error,
        fmt::format( "GLFW minimal version: {}", getMinVersionString() ) //
    };
} // initialize

std::span<const char*>
WindowManager::getRequiredExtensions()
{
    uint32_t size = 0;
    const char** extensions = glfwGetRequiredInstanceExtensions( &size );

    return std::span<const char*>{ extensions, extensions + size };
} // getRequiredExtensions

std::string
WindowManager::getMinVersionString()
{
    return WindowManager::k_api_min_version.to_string();
} // getMinVersionString

} // namespace wnd::glfw