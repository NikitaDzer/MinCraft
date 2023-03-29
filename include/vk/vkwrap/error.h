#pragma once

#include "common/utils.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace vkwrap
{

class Error : public std::runtime_error
{

  public:
    Error( std::string msg )
        : std::runtime_error{ std::move( msg ) }
    {
    }

}; // Error

enum class UnsupportedTag
{
    e_unsupported_extension,
    e_unsupported_layer,
}; // UnsupportedTag

constexpr std::string_view
unsupportedTagToStr( UnsupportedTag tag )
{
    switch ( tag )
    {
    case UnsupportedTag::e_unsupported_extension:
        return "Extension";
    case UnsupportedTag::e_unsupported_layer:
        return "Layer";
    default:
        assert( 0 && "[Debug]: Broken UnsupportedTag enum" );
        std::terminate();
    }
} // unsupportedTagToStr

struct UnsupportedEntry
{
    UnsupportedTag m_tag;
    std::string m_name;
}; // UnsupportedEntry

class UnsupportedError : public Error, private std::vector<UnsupportedEntry>
{
  private:
    using VectorBase = std::vector<UnsupportedEntry>;

  public:
    UnsupportedError( std::string msg, VectorBase missing )
        : Error{ std::move( msg ) },
          VectorBase{ std::move( missing ) }
    {
    }

    using VectorBase::cbegin;
    using VectorBase::cend;
    using VectorBase::empty;
    using VectorBase::size;
    using VectorBase::operator[];

    auto begin() const { return cbegin(); }
    auto end() const { return cend(); }
}; // UnsupportedError

} // namespace vkwrap