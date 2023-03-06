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

#include <GLFW/glfw3.h>

#include <memory>
#include <iostream>
#include <string>
#include <functional>

#include <cassert>

#include "error.h"


namespace wnd
{

class Window;
class WindowManager;

struct WindowConfig;

using WindowType = GLFWwindow;
using FramebufferSizeType = std::pair<int, int>;

using WindowResizeCallbackSignature = 
    void( Window* window, int width, int height );

using ErrorCallbackSignature = 
    void(int error_code, const char* description);


struct WindowConfig
{
    const int   width      = k_default_width;
    const int   height     = k_default_height;
    const char *title      = k_default_title;
    const bool  fullscreen = k_default_fullscreen;

    std::function<WindowResizeCallbackSignature> resize_callback = k_default_resize_callback;


    static constexpr int  k_default_width      = 640;
    static constexpr int  k_default_height     = 480;
    static constexpr char k_default_title[]    = "MinCraft";
    static constexpr bool k_default_fullscreen = false;

    static inline const std::function<WindowResizeCallbackSignature> k_default_resize_callback = 
        []( Window*, int width, int height )
        { 
            std::cout << "Width: " << width << ", height: " << height << std::endl; 
        };
}; // struct WindowConfig


namespace detail
{

using FramebufferSizeCallbackSignature = void(WindowType*, int, int);

class WindowImpl
{

private:
    WindowType* m_window;

    static void setWindowHints()
    {
        glfwDefaultWindowHints();

        glfwWindowHint( GLFW_RESIZABLE,               GLFW_TRUE );
        glfwWindowHint( GLFW_VISIBLE,                 GLFW_TRUE );
        glfwWindowHint( GLFW_FOCUSED,                 GLFW_TRUE );
        glfwWindowHint( GLFW_FLOATING,                GLFW_FALSE );
        glfwWindowHint( GLFW_TRANSPARENT_FRAMEBUFFER, GLFW_FALSE );
        glfwWindowHint( GLFW_FOCUS_ON_SHOW,           GLFW_TRUE );
        glfwWindowHint( GLFW_DOUBLEBUFFER,            GLFW_TRUE );
        glfwWindowHint( GLFW_REFRESH_RATE,            GLFW_DONT_CARE ); 
        glfwWindowHint( GLFW_CLIENT_API,              GLFW_NO_API);
    } // setWindowHints

public:
    WindowImpl( const WindowConfig &config, 
                void *bound_ptr, 
                FramebufferSizeCallbackSignature resize_callback )
    {
        setWindowHints();

        m_window = glfwCreateWindow(
                config.width, 
                config.height, 
                config.title, 
                config.fullscreen ? glfwGetPrimaryMonitor() : NULL,
                NULL);

        glfwSetWindowUserPointer( m_window, bound_ptr );
        glfwSetFramebufferSizeCallback( m_window, resize_callback );

        std::cout << "Create window." << std::endl;
    } // WindowImpl

   ~WindowImpl()
    {
        if ( m_window != NULL )
        {
            glfwDestroyWindow( m_window );
        }

        std::cout << "Delete window." << std::endl;
    } // ~WindowImpl

    // Call permissions: main thread.
    FramebufferSizeType getFramebufferSize() const&
    {
        int width  = 0;
        int height = 0;

        glfwGetFramebufferSize( m_window, &width, &height );

        return std::make_pair( width, height );
    } // getFramebufferSize
 
    // Call permissions: main thread.
    int getFramebufferWidth() const& { return getFramebufferSize().first; }

    // Call permissions: main thread.
    int getFramebufferHeight() const& { return getFramebufferSize().second; }

    // Call permissions: main thread.
    void setFullscreen() const&
    {
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();

        glfwSetWindowMonitor( m_window, monitor, 0, 0, getFramebufferWidth(), getFramebufferHeight(), GLFW_DONT_CARE );
    } // setFullscreen

    // Call permissions: main thread.
    void setWindowed() const&
    {
        constexpr int xpos = 0;
        constexpr int ypos = 0;

        glfwSetWindowMonitor( m_window, NULL, xpos, ypos, getFramebufferWidth(), getFramebufferHeight(), GLFW_DONT_CARE );
    } // setWindowed

    WindowType* const& get() const& { return m_window; }
}; // class WindowImpl

} // namespace detail


class Window
{

private:
    using InnerType = detail::WindowImpl;
    using HandleType = std::unique_ptr<InnerType>;

private:
    HandleType m_handle;
    std::function<WindowResizeCallbackSignature> m_resize_callback;

    // Call permissions: main thread.
    HandleType createHandle( const WindowConfig &config )
    {
        void *bound_ptr = this;
        detail::FramebufferSizeCallbackSignature *callback = framebufferSizeCallback;

        return std::make_unique<InnerType>( config, bound_ptr, callback );
    } // createHandle

    static void framebufferSizeCallback( WindowType* innerWindow, int width, int height )
    {
        Window *window = reinterpret_cast<Window*>( glfwGetWindowUserPointer( innerWindow ) );
        window->m_resize_callback( window, width, height );
    } // framebufferSizeCallback


public:
    Window( const WindowConfig &config = {} ):
        m_handle( createHandle( config ) ),
        m_resize_callback( config.resize_callback )
    {
    }

    // Too lazy to do it...
    Window( const Window &window ) = delete;
    Window( Window &&window ) = delete;

    Window &operator = ( const Window &window ) = delete;
    Window &operator = ( Window &&window ) = delete;


    WindowType* const& get() const& { return m_handle->get(); }

    // Call permissions: main thread.
    FramebufferSizeType getFramebufferSize() const& { return m_handle->getFramebufferSize(); }

    // Call permissions: any thread.
    bool running() const& { return !glfwWindowShouldClose( m_handle->get() ); }

    // Call permissions: main thread.
    int getFramebufferWidth() const& { return m_handle->getFramebufferWidth(); }

    // Call permissions: main thread.
    int getFramebufferHeight() const& { return m_handle->getFramebufferHeight(); }

    // Call permissions: main thread.
    void setWindowed() const& { m_handle->setWindowed(); }

    // Call permissions: main thread.
    void setFullscreen() const& { m_handle->setFullscreen(); }
}; // class Window


class WindowManager
{

private:
    static constexpr int k_min_major = 3;
    static constexpr int k_min_minor = 3;

    WindowManager( ErrorCallbackSignature *error_callback )
    {
        glfwSetErrorCallback( error_callback );
        glfwInit();

        std::cout << "Init glfw." << std::endl;
    } // WindowManager

   ~WindowManager()
    {
        glfwTerminate();

        std::cout << "Terminate glfw." << std::endl;
    } // ~WindowManager

    static void defaultErrorCallback( int error_code, const char* description )
    {
        throw Error( error_code, description );
    } // defaultErrorCallback

    static bool isSuitableVersion()
    {
        int major = 0;
        int minor = 0;

        glfwGetVersion( &major, &minor, NULL );

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


public:
    static const std::string getMinVersionString()
    {
        return std::to_string( k_min_major ) + "." + std::to_string( k_min_minor );
    } // getMinVersionString

    // Call permissions: main thread.
    static void initialize( ErrorCallbackSignature *error_callback = defaultErrorCallback )
    {
        static WindowManager instance( error_callback );

        if ( !isSuitableVersion() )
        {
            throw Error( Error::k_user_error, 
                         std::string( "GLFW minimal version: " ) + getMinVersionString() );
        }
    } // initialize

    // Call permission: any thread, after WindowManager::initialize.
    static const char** getRequiredExtensions( uint32_t* count )
    {
        assert( count != nullptr && "Count pointer is nullptr." );
        return glfwGetRequiredInstanceExtensions( count );
    }

}; // class WindowManager

} // namespace wnd
