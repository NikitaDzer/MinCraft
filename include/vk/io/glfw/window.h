#pragma once

#include <memory>
#include <string>

// Forward declaration to avoid including the GLFW header
class GLFWwindow;

extern "C" void
glfwDestroyWindow( GLFWwindow* );

namespace io::glfw
{

class WindowGLFWUnique
{
  private:
    // glfwDestroyWindow does nothing when passes nullptr, so this class be be default initialized and nothing will
    // break. See: https://github.com/glfw/glfw/blob/3.3.8/src/window.c#L425-L427
    std::unique_ptr<GLFWwindow, decltype( &glfwDestroyWindow )> m_handle;

  private:
    static GLFWwindow* createWindow( const std::string& name, unsigned width, unsigned height, bool resizable );

  public:
    WindowGLFWUnique( GLFWwindow* raw_handle = nullptr )
        : m_handle{ raw_handle, glfwDestroyWindow }
    {
    }

    WindowGLFWUnique( const std::string& name, unsigned width, unsigned height, bool resizable = false )
        : WindowGLFWUnique{ createWindow( name, width, height, resizable ) }
    {
    }
};

} // namespace io::glfw