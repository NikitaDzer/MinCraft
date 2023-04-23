#pragma once

#include "common/vulkan_include.h"

#include "vkwrap/core.h"
#include "vkwrap/mman.h"
#include "vkwrap/utils.h"

#include "utils/patchable.h"

#include <range/v3/range/concepts.hpp>
#include <range/v3/range/conversion.hpp>

#include <functional>
#include <optional>
#include <utility>

#include <cassert>

namespace vkwrap
{

class Buffer : private vk::Buffer
{

  private:
    using Base = vk::Buffer;

  private:
    Mman* m_mman{ nullptr };

    static vk::Buffer createBuffer( const vk::BufferCreateInfo& create_info, Mman& mman )
    {
        return mman.create( create_info );
    } // createBuffer

    friend void swap( Buffer& lhs, Buffer& rhs )
    {
        std::swap( static_cast<Base&>( lhs ), static_cast<Base&>( rhs ) );
        std::swap( lhs.m_mman, rhs.m_mman );
    } // swap

  public:
    Buffer( const vk::BufferCreateInfo& create_info, Mman& mman )
        : Base{ createBuffer( create_info, mman ) },
          m_mman{ &mman }
    {
    } // Buffer

    ~Buffer()
    {
        /** There cannot be an exception.
         *
         * 1. If `createBuffer` throws an exception in constructor,
         * then `m_mman == nullptr` and we won't `destroy` an object.
         *
         * 2. If constructor completes successfully,
         * then `Mman::findInfo` CANNOT throw an exception:
         * - info must be found in `Mman::m_buffers_info`
         * - as `vk::Buffer` and `Mman::BufferInfo` have only noexcept methods,
         *   then `unordered_map::erase` cannot throw an exception
         */
        if ( m_mman != nullptr )
        {
            m_mman->destroy( *this );
        }
    } // ~Buffer

    Buffer( Buffer&& image )
        : Base{ nullptr },
          m_mman{ nullptr }
    {
        swap( *this, image );
    } // Buffer( Buffer&& )

    Buffer& operator=( Buffer&& image )
    {
        Buffer tmp{ std::move( image ) };
        swap( *this, tmp );

        return *this;
    } // operator = ( Buffer&& )

    Buffer( const Buffer& ) = delete;
    Buffer& operator=( const Buffer& ) = delete;

    // TODO: make it more type-safe.
    void update( auto* raw_data, size_t n_bytes )
    {
        assert( raw_data );

        if ( raw_data != nullptr )
        {
            uint8_t* src = reinterpret_cast<uint8_t*>( raw_data );
            uint8_t* dst = m_mman->map( *this );

            std::copy( src, src + n_bytes, dst );
            m_mman->flush( *this );

            m_mman->unmap( *this );
        }
    } // update

    void update( ranges::contiguous_range auto&& range ) { update( range.data(), range.size() ); }
    void update( vk::Buffer src_buffer ) { m_mman->copy( src_buffer, *this ); }

    Base& get() { return static_cast<Base&>( *this ); }
    const Base& get() const { return static_cast<const Base&>( *this ); }

    using Base::operator bool;
    using Base::operator VkBuffer;

}; // class Buffer

class BufferBuilder
{

  public:
    using QueueFamilyIndices = std::vector<QueueFamilyIndex>;

    // clang-format off
    PATCHABLE_DEFINE_STRUCT( 
        BufferPartialInfo,
        ( std::optional<vk::DeviceSize>,       size    ),
        ( std::optional<vk::BufferUsageFlags>, usage   ),
        ( std::optional<QueueFamilyIndices>,   indices )
    );
    // clang-format on

    using Setter = std::function<void( BufferPartialInfo& )>;

  private:
    // clang-format off
    Setter m_setter = []( auto& ){};
    // clang-format on

    BufferPartialInfo m_partial;

    BufferPartialInfo makePartialInfo() const
    {
        BufferPartialInfo partial{};

        presetter( partial );
        m_setter( partial );
        partial.patchWith( m_partial );

        return partial;
    } // makePartialInfo

    static constexpr vk::BufferCreateInfo k_initial_create_info = {
        // We don't use these specific fields.
        .pNext = {},
        .flags = {},
    };

  public:
    // clang-format off
    static inline Setter presetter = []( auto& ){};
    // clang-format on

    BufferBuilder() = default;

    BufferBuilder& withSetter( Setter setter ) &
    {
        m_setter = setter;
        return *this;
    } // withSetter

    BufferBuilder& withSize( vk::DeviceSize size ) &
    {
        m_partial.size = size;
        return *this;
    } // withSize

    BufferBuilder& withUsage( vk::BufferUsageFlags usage ) &
    {
        m_partial.usage = usage;
        return *this;
    } // withUsage

    BufferBuilder& withQueueFamilyIndices( ranges::range auto&& indices ) &
    {
        m_partial.indices = ranges::to_vector( indices );
        return *this;
    } // withQueueFamilyIndices

    Buffer make( Mman& mman ) const&
    {
        BufferPartialInfo partial = makePartialInfo();
        partial.assertCheckMembers();
        assert( !partial.indices->empty() );

        vk::BufferCreateInfo create_info{ k_initial_create_info };
        create_info.size = *partial.size;
        create_info.usage = *partial.usage;

        SharingInfoSetter setter{ partial.indices.value() };
        setter.setTo( create_info );

        return { create_info, mman };
    } // make

}; // class BufferBuilder

} // namespace vkwrap
