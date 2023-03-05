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
        : std::runtime_error{ msg }
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
        : Error{ msg },
          VectorBase{ std::move( missing ) }
    {
    }

    using VectorBase::cbegin;
    using VectorBase::cend;
    using VectorBase::empty;
    using VectorBase::size;

    auto begin() const { return cbegin(); }
    auto end() const { return cend(); }

    decltype( auto ) operator[]( VectorBase::size_type index ) { return VectorBase::operator[]( index ); };
}; // UnsupportedError

} // namespace vkwrap