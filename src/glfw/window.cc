#include <functional>
#include <iostream>
#include <memory>
#include <string>

#include <GLFW/glfw3.h>
#include <spdlog/spdlog.h>

#include "window/error.h"
#include "window/window.h"

using namespace wnd;

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

std::string
getHandleAddressString( const Window* window )
{
    return std::to_string( reinterpret_cast<uint64_t>( window ) );
} // getHandleAddressString

void
logGLFWaction(
    const std::string& action,
    const std::string& handleAddressString = {},
    const std::string& additional = {} //
)
{
    std::string output = "GLFW " + action + ".";

    if ( !handleAddressString.empty() )
    {
        // clang-format off
        output += "\n"
                  "Handle = " + handleAddressString;
        // clang-format on

        if ( !additional.empty() )
        {
            output += ", " + additional + ".";
        }
    }

    spdlog::info( output );
} // logGLFWaction

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
        config.title,
        config.fullscreen ? glfwGetPrimaryMonitor() : nullptr,
        nullptr );

    glfwSetWindowUserPointer( glfwWindow, bound_handle );
    glfwSetFramebufferSizeCallback( glfwWindow, framebufferSizeCallback );

    logGLFWaction(
        "create window",
        getHandleAddressString( bound_handle ) //
    );

    return glfwWindow;
} // createWindow

void
Window::destroyWindow( WindowType* glfwWindow )
{
    Window* window = getHandle( glfwWindow );

    glfwDestroyWindow( glfwWindow );

    logGLFWaction(
        "destroy window",
        getHandleAddressString( window ) //
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
        "framebuffer resize",
        getHandleAddressString( window ),
        "width = " + std::to_string( width ) + ", height = " + std::to_string( height ) //
    );

    window->m_resize_callback( window, width, height );
} // framebufferSizeCallback

FramebufferSizeType
Window::getFramebufferSize() const&
{
    int width = 0;
    int height = 0;

    glfwGetFramebufferSize( m_handle.get(), &width, &height );

    return std::make_pair( width, height );
} // getFramebufferSize

void
Window::setWindowed() const&
{
    constexpr int xpos = 0;
    constexpr int ypos = 0;

    glfwSetWindowMonitor(
        m_handle.get(),
        nullptr,
        xpos,
        ypos,
        getFramebufferWidth(),
        getFramebufferHeight(),
        GLFW_DONT_CARE );

    logGLFWaction(
        "choose windowed mode",
        getHandleAddressString( this ) //
    );
} // setWindowed

void
Window::setFullscreen() const&
{
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();

    glfwSetWindowMonitor(
        m_handle.get(),
        monitor,
        0,
        0,
        getFramebufferWidth(),
        getFramebufferHeight(),
        GLFW_DONT_CARE );

    logGLFWaction( "choose fullscreen mode", getHandleAddressString( this ) );
} // setFullscreen

WindowManager::WindowManager( ErrorCallbackSignature* error_callback )
{
    glfwSetErrorCallback( error_callback );
    glfwInit();

    logGLFWaction( "initialize library" );
} // WindowManager

WindowManager::~WindowManager()
{
    glfwTerminate();

    logGLFWaction( "terminate library" );
} // ~WindowManager

bool
WindowManager::isSuitableVersion()
{
    int major = 0;
    int minor = 0;

    glfwGetVersion( &major, &minor, nullptr );

    if ( major > k_min_major )
    {
        return true;
    }

    if ( major == k_min_major && minor >= k_min_minor )
    {
        return true;
    }

    return false;
} // isSuitableVersion

void
WindowManager::initialize( ErrorCallbackSignature* error_callback )
{
    static WindowManager instance( error_callback );

    if ( !isSuitableVersion() )
    {
        throw Error(
            Error::k_user_error,
            "GLFW minimal version: " + getMinVersionString() + "." //
        );
    }
} // initialize

std::span<std::string_view>
WindowManager::getRequiredExtensions()
{
    uint32_t size = 0;
    const char** extensions = glfwGetRequiredInstanceExtensions( &size );

    static std::vector<std::string_view> vectorized{ extensions, extensions + size };

    return vectorized;
} // getRequiredExtensions
