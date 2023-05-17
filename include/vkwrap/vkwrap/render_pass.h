#pragma once

#include "common/vulkan_include.h"

#include <range/v3/algorithm/copy.hpp>
#include <range/v3/iterator/insert_iterators.hpp>
#include <range/v3/range/concepts.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/all.hpp>
#include <range/v3/view/iota.hpp>
#include <range/v3/view/transform.hpp>

#include <concepts>
#include <vector>

namespace vkwrap
{

class RenderPassBuilder
{
  public:
    RenderPassBuilder& withColorAttachment( vk::Format swapchain_format ) &
    {
        auto attachment = vk::AttachmentDescription{
            .format = swapchain_format,
            .samples = vk::SampleCountFlagBits::e1,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eStore,
            .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
            .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
            .initialLayout = vk::ImageLayout::eUndefined,
            .finalLayout = vk::ImageLayout::ePresentSrcKHR };

        return withColorAttachment( attachment );
    };

    RenderPassBuilder& withColorAttachment( vk::AttachmentDescription attachment ) &
    {
        m_color_attachments.push_back( attachment );
        return *this;
    };

    RenderPassBuilder& withDepthAttachment( vk::Format depth_format ) &
    {
        m_depth_attachment = vk::AttachmentDescription{
            .format = depth_format,
            .samples = vk::SampleCountFlagBits::e1,
            .loadOp = vk::AttachmentLoadOp::eClear,
            .storeOp = vk::AttachmentStoreOp::eDontCare,
            .stencilLoadOp = vk::AttachmentLoadOp::eDontCare,
            .stencilStoreOp = vk::AttachmentStoreOp::eDontCare,
            .initialLayout = vk::ImageLayout::eUndefined,
            .finalLayout = vk::ImageLayout::eDepthStencilAttachmentOptimal };

        return *this;
    }

    template <ranges::range Range>
        requires std::same_as<ranges::range_value_t<Range>, vk::SubpassDependency>
    RenderPassBuilder& withSubpassDependencies( Range&& dependecies )
    {
        ranges::copy( dependecies, ranges::back_inserter( m_subpass_dependecies ) );
        return *this;
    }

    vk::UniqueRenderPass make( vk::Device device )
    {
        const auto create_attachment_reference = []( auto index ) {
            return vk::AttachmentReference{ .attachment = index, .layout = vk::ImageLayout::eColorAttachmentOptimal };
        };

        auto color_attachment_references = ranges::views::iota( uint32_t{ 0 }, m_color_attachments.size() ) |
            ranges::views::transform( create_attachment_reference ) | ranges::to_vector;

        const auto depth_attachment_reference = vk::AttachmentReference{
            .attachment = static_cast<uint32_t>( m_color_attachments.size() ),
            .layout = vk::ImageLayout::eDepthStencilAttachmentOptimal };

        const auto subpass = vk::SubpassDescription{
            .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
            .colorAttachmentCount = static_cast<uint32_t>( color_attachment_references.size() ),
            .pColorAttachments = color_attachment_references.data(),
            .pDepthStencilAttachment = &depth_attachment_reference };

        auto attachments = std::vector<vk::AttachmentDescription>{};
        ranges::copy( m_color_attachments, ranges::back_inserter( attachments ) );
        attachments.push_back( m_depth_attachment );

        vk::RenderPassCreateInfo render_pass_create_info{
            .attachmentCount = static_cast<uint32_t>( attachments.size() ),
            .pAttachments = attachments.data(),
            .subpassCount = 1,
            .pSubpasses = &subpass,
            .dependencyCount = static_cast<uint32_t>( m_subpass_dependecies.size() ),
            .pDependencies = m_subpass_dependecies.data() };

        return device.createRenderPassUnique( render_pass_create_info );
    }

  public:
    RenderPassBuilder() = default;

  private:
    std::vector<vk::SubpassDependency> m_subpass_dependecies;
    std::vector<vk::AttachmentDescription> m_color_attachments;
    vk::AttachmentDescription m_depth_attachment;
};

}; // namespace vkwrap