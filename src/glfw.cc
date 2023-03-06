#include "io/glfw/core.h"
#include "io/glfw/window.h"

#include <GLFW/glfw3.h>

namespace io::glfw
{

void
checkGLFWError()
{
    const char* description = nullptr;
    const auto error_code = glfwGetError( &description );

    if ( error_code == GLFW_NO_ERROR )
        return;

    throw glfw::Error{ error_code, description };
} // checkGLFWError

static void
throwingGLFWErrorCallback( int error_code, const char* error_msg )
{
    throw glfw::Error{ error_code, error_msg };
} // throwingGLFWErrorCallback

void
enableGLFWExceptions()
{
    glfwSetErrorCallback( throwingGLFWErrorCallback );
    checkGLFWError();
}

GLFWwindow*
WindowGLFWUnique::createWindow( const std::string& name, unsigned width, unsigned height, bool resizable )
{
    return nullptr;
}

} // namespace io::glfw