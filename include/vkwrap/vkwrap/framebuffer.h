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

namespace vkwrap
{

class Framebuffer : private vk::UniqueFramebuffer
{

  private:
    using Base = vk::UniqueFramebuffer;

  public:
    Framebuffer( vk::UniqueFramebuffer framebuffer )
        : Base{ std::move( framebuffer ) }
    {
    }

    using Base::operator bool;
    using Base::operator->;
    using Base::get;

}; // class Framebuffer

class FramebufferBuilder
{

  public:
    // clang-format off
    PATCHABLE_DEFINE_STRUCT( 
        FramebufferPartialInfo,
        ( std::optional<vk::RenderPass>,               render_pass ),
        ( std::optional< std::vector<vk::ImageView> >, attachments ),
        ( std::optional<uint32_t>,                     width       ),
        ( std::optional<uint32_t>,                     height      )
    );
    // clang-format on

    using Setter = std::function<void( FramebufferPartialInfo& )>;

  private:
    // clang-format off
    Setter m_setter = []( auto& ){};
    // clang-format on

    FramebufferPartialInfo m_partial;

    FramebufferPartialInfo makePartialInfo() const
    {
        FramebufferPartialInfo partial{};

        presetter( partial );
        m_setter( partial );
        partial.patchWith( m_partial );

        return partial;
    } // makePartialInfo

    // clang-format off
    static constexpr vk::FramebufferCreateInfo k_framebuffer_initial_create_info{ 
        // We don't use these specific fields.
        .pNext = {},
        .flags = {},

        // We use images with only one layer.
        .layers = 1 
    };
    // clang-format on

  public:
    // clang-format off
    static inline Setter presetter = []( auto& ){};
    // clang-format on

    FramebufferBuilder() = default;

    FramebufferBuilder& withSetter( Setter setter ) &
    {
        m_setter = setter;
        return *this;
    } // withSetter

    FramebufferBuilder& withRenderPass( vk::RenderPass render_pass ) &
    {
        m_partial.render_pass = render_pass;
        return *this;
    } // withRenderPass

    FramebufferBuilder& withAttachments( ranges::range auto&& attachments ) &
    {
        m_partial.attachments = ranges::to_vector( attachments );
        return *this;
    } // withAttachments

    FramebufferBuilder& withWidth( uint32_t width ) &
    {
        m_partial.width = width;
        return *this;
    } // withWidth

    FramebufferBuilder& withHeight( uint32_t height ) &
    {
        m_partial.height = height;
        return *this;
    } // withHeight

    Framebuffer make( vk::Device device ) const&
    {
        FramebufferPartialInfo partial = makePartialInfo();
        partial.assertCheckMembers();

        vk::FramebufferCreateInfo create_info{ k_framebuffer_initial_create_info };
        create_info.setRenderPass( *partial.render_pass );

        auto unique_attachments = utils::getUniqueElements( m_partial.attachments.value() );
        create_info.setAttachments( *m_partial.attachments );

        create_info.setWidth( *partial.width );
        create_info.setHeight( *partial.height );

        return { device.createFramebufferUnique( create_info ) };
    } // make

}; // class FramebufferBuilder

} // namespace vkwrap
