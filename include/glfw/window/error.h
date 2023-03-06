#pragma once

#include <stdexcept>

namespace wnd 
{

class Error : public std::runtime_error
{

private:
    int m_error_code;

public:
    static constexpr int k_user_error = 0;

    Error( int error_code, std::string description ):
        std::runtime_error( description ),
        m_error_code( error_code )
    {}

    int getErrorCode() const { return m_error_code; }
}; // class Error

} // namespace wnd
