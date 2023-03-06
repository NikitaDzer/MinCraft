#pragma once

#include <stdexcept>
#include <string>

namespace io::glfw
{

class Error : public std::runtime_error
{
  private:
    int m_error_code;

  public:
    Error( int error_code, std::string error_msg )
        : std::runtime_error{ error_msg },
          m_error_code{ error_code }
    {
    }

    int error_code() const { return m_error_code; }
};

// Call to check if any GLFW error occured. If it did, then throws a glfw::Error with the corresponding
// description.
void
checkGLFWError();

// Enable default callback to throw exceptions on any GLFW error.
void
enableGLFWExceptions();

} // namespace io::glfw