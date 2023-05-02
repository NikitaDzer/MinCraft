#pragma once

#include <boost/hana.hpp>

#include <spdlog/fmt/bundled/core.h>

#include <cassert>
#include <iostream>

/** How to declare an "patchable" struct?
 *
 *  Syntax:
 *
 *  PATCHABLE_DEFINE_STRUCT(
 *      <Struct name>,                                 -------------------------------------
 *      ( <member type>, <member name> ), <----------- | Comma between member declarations |
 *      ...                                            -------------------------------------
 *      ( <member type>, <member name> )
 *  );                 ^                ^
 *                     |                |
 *      ---------------|----------------|-----------------
 *      | comma between member | no comma after the last |
 *      | type and name        | member declaration      |
 *      --------------------------------------------------
 *
 *  Member type must contain method has_value,
 *  that checks is member set.
 *  In most cases you would use std::optional.
 *
 *  Example of declaration:
 *
 *  PATCHABLE_DEFINE_STRUCT(
 *      Person,
 *      ( std::optional<std::string>, name ),
 *      ( std::optional<int>, age )
 *  );
 */

#ifndef NDEBUG

#define ADD_METHOD_ASSERT_CHECK_MEMBERS( struct_name )                                                                 \
    void assertCheckMembers() const                                                                                    \
    {                                                                                                                  \
        boost::hana::for_each( getAccessors(), [ & ]( const auto& accessor ) {                                         \
            if ( !accessMember( accessor ).has_value() )                                                               \
            {                                                                                                          \
                const auto msg = fmt::format(                                                                          \
                    "Assertion failed: {}::{} has no value.\n",                                                        \
                    #struct_name,                                                                                      \
                    getMemberName( accessor ) );                                                                       \
                                                                                                                       \
                std::cerr << msg;                                                                                      \
                std::abort();                                                                                          \
            }                                                                                                          \
        } );                                                                                                           \
    } /* assertCheckMembers */

#else // !NDEBUG

#define ADD_METHOD_ASSERT_CHECK_MEMBERS( struct_name )                                                                 \
    void assertCheckMembers() const                                                                                    \
    {                                                                                                                  \
    }

#endif // !NDEBUG

#define PATCHABLE_DEFINE_STRUCT( struct_name, ... )                                                                    \
    struct struct_name                                                                                                 \
    {                                                                                                                  \
        BOOST_HANA_DEFINE_STRUCT( struct_name, __VA_ARGS__ );                                                          \
                                                                                                                       \
      private:                                                                                                         \
        auto& accessMember( const auto& accessor )                                                                     \
        {                                                                                                              \
            return boost::hana::second( accessor )( *this );                                                           \
        } /* accessMember */                                                                                           \
                                                                                                                       \
        const auto& accessMember( const auto& accessor ) const&                                                        \
        {                                                                                                              \
            return boost::hana::second( accessor )( *this );                                                           \
        } /* accessMember */                                                                                           \
                                                                                                                       \
        static constexpr auto getMemberName( const auto& accessor )                                                    \
        {                                                                                                              \
            return boost::hana::to<const char*>( boost::hana::first( accessor ) );                                     \
        } /* getMemberName */                                                                                          \
                                                                                                                       \
        static constexpr auto getAccessors()                                                                           \
        {                                                                                                              \
            return boost::hana::accessors<struct_name>();                                                              \
        } /* getAccessors */                                                                                           \
                                                                                                                       \
      public:                                                                                                          \
        ADD_METHOD_ASSERT_CHECK_MEMBERS( struct_name );                                                                \
                                                                                                                       \
        void patchWith( const struct_name& patchable )                                                                 \
        {                                                                                                              \
            const auto members = boost::hana::members( patchable );                                                    \
            const auto members_count = boost::hana::size( members );                                                   \
            const auto members_indices = boost::hana::range_c<size_t, 0, members_count>;                               \
                                                                                                                       \
            boost::hana::for_each( members_indices, [ & ]( auto i ) {                                                  \
                if ( boost::hana::at_c<i>( members ).has_value() )                                                     \
                {                                                                                                      \
                    constexpr auto accessor = getAccessors()[ boost::hana::size_c<i> ];                                \
                    accessMember( accessor ) = boost::hana::at_c<i>( members );                                        \
                }                                                                                                      \
            } );                                                                                                       \
        } /* patchWith */                                                                                              \
    }
