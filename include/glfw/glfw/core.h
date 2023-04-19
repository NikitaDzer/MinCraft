#pragma once

#include "common/glfw_include.h"

#include <spdlog/fmt/bundled/format.h>
#include <spdlog/spdlog.h>

#include <range/v3/view/iota.hpp>
#include <range/v3/view/take.hpp>

#include <range/v3/algorithm/any_of.hpp>
#include <range/v3/view/all.hpp>
#include <range/v3/view/filter.hpp>

#include "utils/misc.h"

#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>

namespace glfw
{

enum class ErrorCode : int
{
    e_no_error = GLFW_NO_ERROR,
    e_not_initialized = GLFW_NOT_INITIALIZED,
    e_no_current_context = GLFW_NO_CURRENT_CONTEXT,
    e_invalid_enum = GLFW_INVALID_ENUM,
    e_invalid_value = GLFW_INVALID_VALUE,
    e_out_of_memory = GLFW_OUT_OF_MEMORY,
    e_api_unavailable = GLFW_API_UNAVAILABLE,
    e_version_unavailable = GLFW_VERSION_UNAVAILABLE,
    e_platform_error = GLFW_PLATFORM_ERROR,
    e_format_unavailable = GLFW_FORMAT_UNAVAILABLE,
    e_no_window_context = GLFW_NO_WINDOW_CONTEXT,
    e_user_error, // Used for other errors reserved for this wrapper library
};                // enum class ErrorCode

// Utility to get an error description. Messages taken from documentation:
// https://www.glfw.org/docs/3.3/group__errors.html
inline std::string_view
errorCodeToString( ErrorCode error_code )
{
    switch ( error_code )
    {
    case ErrorCode::e_no_error:
        return "No error has occurred";
    case ErrorCode::e_not_initialized:
        return "API has not been initialized";
    case ErrorCode::e_no_current_context:
        return "No context is current for this thread";
    case ErrorCode::e_invalid_enum:
        return "One of the arguments to the function was an invalid enum value";
    case ErrorCode::e_invalid_value:
        return "One of the arguments to the function was an invalid value";
    case ErrorCode::e_out_of_memory:
        return "A memory allocation failed";
    case ErrorCode::e_api_unavailable:
        return "Could not find support for the requested API on the system";
    case ErrorCode::e_version_unavailable:
        return "The requested OpenGL or OpenGL ES version is not available";
    case ErrorCode::e_platform_error:
        return "A platform-specific error occurred that does not match any of the more specific categories";
    case ErrorCode::e_format_unavailable:
        return "The requested format is not supported or available";
    case ErrorCode::e_no_window_context:
        return "The specified window does not have an OpenGL or OpenGL ES context";
    case ErrorCode::e_user_error:
        return "A user error has occured";
    default:
        assert( 0 && "Unhandled enum case" );
        std::terminate();
        break;
    }
} // errorCodeToString

class Error : public std::runtime_error
{
  private:
    ErrorCode m_error_code;

  public:
    Error( ErrorCode error_code, std::string description )
        : std::runtime_error{ std::move( description ) },
          m_error_code{ error_code }
    {
    }

    ErrorCode getErrorCode() const { return m_error_code; }
}; // class Error

inline void
checkError()
{
    auto get_error = []() {
        // As per the documentation: The returned string is allocated and freed by GLFW. You should not free it
        // yourself. It is guaranteed to be valid only until the next error occurs or the library is terminated.
        // For this reason it's prudent to create a copy of the data so that this pointer does not dangle.
        const char* temporary_message;
        auto ec = glfwGetError( &temporary_message );
        return std::pair{ static_cast<ErrorCode>( ec ), temporary_message };
    };

    auto [ error_code, message ] = get_error();
    if ( error_code != ErrorCode::e_no_error )
    {
        throw Error{ error_code, message };
    }
}

namespace detail
{

inline void
throwOnErrorCallback( int error_code, const char* message )
{
    throw ::glfw::Error{ static_cast<ErrorCode>( error_code ), message };
} // throwOnErrorCallback

inline void
enableExceptions()
{
    glfwSetErrorCallback( throwOnErrorCallback );
} // enableExceptions

inline void
logAction(
    const std::string& action,
    const void* handle = nullptr,
    const std::vector<std::string>& additional = {} //
)
{
    std::stringstream ss;

    if ( handle )
    {
        ss << fmt::format( "Message (GLFW) [handle = {}]: {}", fmt::ptr( handle ), action );
    } else
    {
        ss << fmt::format( "Message (GLFW): {}", action );
    }

    if ( !additional.empty() )
    {
        ss << fmt::format( "\n -- Additional info --" );
        for ( auto i : ranges::views::iota( uint32_t{ 0 } ) | ranges::views::take( additional.size() ) )
        {
            ss << fmt::format( "\n[{}]. {}", i, additional[ i ] );
        }
    }

    ss << "\n";
    spdlog::info( ss.str() );
} // logGLFWaction

}; // namespace detail

struct Version
{
    int major = 0;
    int minor = 0;
    int revision = 0;

  public:
    constexpr auto operator<=>( const Version& ) const = default;
    std::string to_string() const { return fmt::format( "{}.{}.{}", major, minor, revision ); }
}; // Version

constexpr auto k_current_min_version = Version{ GLFW_VERSION_MAJOR, GLFW_VERSION_MINOR, GLFW_VERSION_REVISION };
constexpr auto k_api_min_version = Version{ 3, 3 };

// Compile error because we are linking statically
static_assert(
    k_current_min_version >= k_api_min_version && "GLFW Library version does not fit the minimum requirements" );

class Instance
{
  public:
    Instance()
    {
        static std::once_flag init_called;
        std::call_once( init_called, initialize );
    }

    Instance( const Instance& ) = delete;
    Instance( Instance&& ) = delete;

    Instance& operator=( const Instance& ) = delete;
    Instance& operator=( Instance&& ) = delete;

    ~Instance()
    {
        // This in incredibly ugly, but since exceptions shouldn't escape destructors this is the only reasonable
        // solution I can come up with.
        try
        {
            terminate();
        } catch ( ... )
        {
        }
    } // ~Instance

  private:
    static void initialize()
    {
        detail::enableExceptions();
        glfwInit();
        detail::logAction( "Initialize library" );
    } // initialize

    static void terminate()
    {
        glfwTerminate();
        detail::logAction( "Terminate library" );
    } // terminate

  public:
    // First: note that this is a member function, thus it can't be called without an Instance object. This means that
    // the library is initialized. Second: this range does not dangle until the GLFW has been terminated. From the docs:
    // -- The returned array is allocated and freed by GLFW. You should not free it yourself. It is guaranteed to be
    // valid only until the library is terminated. --
    std::span<const char*> getRequiredExtensions() const
    {
        uint32_t size = 0;
        const char** extensions = glfwGetRequiredInstanceExtensions( &size );
        return std::span<const char*>{ extensions, extensions + size };
    } // getRequiredExtensions

    // Version related stuff.
  public:
    Version getVersion() const
    {
        Version version;
        glfwGetVersion( &version.major, &version.minor, &version.revision );
        return version;
    } // getVersion

}; // Instance

namespace detail
{

template <typename T> class GlobalHandlerTable
{
  public:
    template <typename Callable>
    T& lookup( GLFWwindow* window, Callable create_function ) // clang-format off
        requires requires () {
        { create_function() } -> std::same_as<T>;
    }
    {
        std::call_once( initialized, [ this ]() { handler_map = std::make_unique<HandlerMap>(); } );
        assert( handler_map );

        auto g = std::unique_lock{ mutex };

        if ( auto handler = handler_map->find( window ); handler != handler_map->end() )
        {
            assert( handler->second );
            return handler->second;
        }

        auto handler = create_function();
        return handler_map->emplace( window, std::move( handler ) ).first->second;
    }

private:
    using HandlerMap = std::unordered_map<GLFWwindow *, T>;

  private:
    std::once_flag initialized;
    std::mutex mutex;
    std::unique_ptr<HandlerMap> handler_map;
}; // GlobalHandlerTable

} // namespace detail

} // namespace glfw