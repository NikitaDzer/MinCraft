#include <spdlog/spdlog.h>

#include "utils/misc.h"

#include "common/glfw_include.h"
#include "glfw/core.h"
#include "glfw/window.h"

#include <range/v3/view/iota.hpp>
#include <range/v3/view/take.hpp>

#include <bit>
#include <functional>
#include <iostream>
#include <iterator>
#include <memory>
#include <string>

namespace glfw::wnd
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

} // namespace

namespace detail
{

WindowHandle*
WindowImpl::createWindow(
    const WindowConfig& config,
    WindowImpl* bound_handle //
)
{
    setWindowHints();

    WindowHandle* glfw_window = glfwCreateWindow(
        config.width,
        config.height,
        config.title.c_str(),
        config.fullscreen ? glfwGetPrimaryMonitor() : nullptr,
        nullptr );

    glfwSetWindowUserPointer( glfw_window, bound_handle );
    glfwSetFramebufferSizeCallback( glfw_window, framebufferSizeCallback );

    glfw::detail::logAction(
        "Create Window",
        glfw_window //
    );

    return glfw_window;
} // createWindow

void
WindowImpl::destroyWindow( WindowHandle* glfw_window )
{
    glfwDestroyWindow( glfw_window );

    glfw::detail::logAction(
        "Destroy Window",
        glfw_window //
    );
} // destroyWindow

void
WindowImpl::framebufferSizeCallback(
    WindowHandle* glfw_window,
    int width,
    int height //
)
{
    WindowImpl& window = getHandle( glfw_window );

    ::glfw::detail::logAction(
        "Framebuffer resize",
        glfw_window,
        { fmt::format( "Width = {}", width ), fmt::format( "Height = {}", height ) } //
    );

    window.m_resize_callback( width, height );
} // framebufferSizeCallback

FramebufferSizeType
WindowImpl::getFramebufferSize() const
{
    FramebufferSizeType size;
    glfwGetFramebufferSize( m_handle.get(), &size.width, &size.height );
    return size;
} // getFramebufferSize

void
WindowImpl::setWindowed()
{
    auto [ width, height ] = getFramebufferSize();

    glfwSetWindowMonitor(
        m_handle.get(),
        nullptr,
        0,
        0,
        width,
        height,
        GLFW_DONT_CARE //
    );

    ::glfw::detail::logAction( "Set windowed mode", m_handle.get() );
} // setWindowed

void
WindowImpl::setFullscreen()
{
    GLFWmonitor* monitor = glfwGetPrimaryMonitor();
    auto [ width, height ] = getFramebufferSize();

    glfwSetWindowMonitor(
        m_handle.get(),
        monitor,
        GLFW_DONT_CARE,
        GLFW_DONT_CARE,
        width,
        height,
        GLFW_DONT_CARE //
    );

    ::glfw::detail::logAction( "Set fullscreen mode", m_handle.get() );
} // setFullscreen

::wnd::UniqueSurface
WindowImpl::createSurface( const vk::Instance& instance ) const
{
    assert( instance );

    VkSurfaceKHR surface;
    glfwCreateWindowSurface(
        static_cast<VkInstance>( instance ),
        m_handle.get(),
        nullptr,
        &surface //
    );

    // Very crucial to pass instance for the correct deleter with dynamic dispatch to work. This is the
    // same reason why all files need to be compiled together.
    return vk::UniqueSurfaceKHR{ surface, instance };
} // createWindowSurface

} // namespace detail

} // namespace glfw::wnd