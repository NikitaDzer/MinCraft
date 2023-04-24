/*
 * ----------------------------------------------------------------------------
 * "THE BEER-WARE LICENSE" (Revision 42):
 * <tsimmerman.ss@phystech.edu>, <alex.rom23@mail.ru> wrote this file.  As long as you
 * retain this notice you can do whatever you want with this stuff. If we meet
 * some day, and you think this stuff is worth it, you can buy us a beer in
 * return.
 * ----------------------------------------------------------------------------
 */

/* Temporary file to fit in place of a proper wrapper for Swapchain */

#pragma once

#include "common/vulkan_include.h"

#include "vkwrap/queues.h"

#include <range/v3/algorithm/find.hpp>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/range/concepts.hpp>

#include <algorithm>
#include <concepts>
#include <cstddef>

namespace vkwrap
{

class Swapchain : private vk::UniqueSwapchainKHR
{
  private:
    template <ranges::range Range>
        requires std::same_as<ranges::range_value_t<Range>, vk::SurfaceFormatKHR>
    static vk::SurfaceFormatKHR chooseSwapchainSurfaceFormat( Range&& formats )
    {
        const auto format_it = ranges::find_if( formats, []( vk::SurfaceFormatKHR format ) {
            return (
                format.format == vk::Format::eB8G8R8A8Unorm && format.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear );
        } );

        if ( format_it != formats.end() )
        {
            return *format_it;
        }

        return *formats.begin();
    }

    template <ranges::range Range>
        requires std::same_as<ranges::range_value_t<Range>, vk::PresentModeKHR>
    static vk::PresentModeKHR chooseSwapchainPresentMode( Range&& present_modes )
    {
        const auto present_mode_it = ranges::find( present_modes, vk::PresentModeKHR::eMailbox );

        if ( present_mode_it != present_modes.end() )
        {
            return *present_mode_it;
        }

        return vk::PresentModeKHR::eFifo;
    }

    static vk::Extent2D chooseSwapchainExtent( vk::Extent2D p_extent, const vk::SurfaceCapabilitiesKHR& p_cap )
    {
        // In some systems UINT32_MAX is a flag to say that the size has not been specified
        if ( p_cap.currentExtent.width != UINT32_MAX )
        {
            return p_cap.currentExtent;
        }

        const auto extent = vk::Extent2D{
            std::min( p_cap.maxImageExtent.width, std::max( p_cap.minImageExtent.width, p_extent.width ) ),
            std::min( p_cap.maxImageExtent.height, std::max( p_cap.minImageExtent.height, p_extent.height ) ) };

        return extent;
    }

    static constexpr auto k_subrange_info = vk::ImageSubresourceRange{
        .aspectMask = vk::ImageAspectFlagBits::eColor,
        .baseMipLevel = 0,
        .levelCount = 1,
        .baseArrayLayer = 0,
        .layerCount = 1 };

  private:
    using Base = vk::UniqueSwapchainKHR;

  public:
    Swapchain() = default;

    Swapchain(
        vk::PhysicalDevice physical_device,
        vk::Device logical_device,
        vk::SurfaceKHR surface,
        vk::Extent2D extent,
        vkwrap::Queue graphics_queue,
        vkwrap::Queue present_queue,
        vk::SwapchainKHR old_swapchain = {} )
        : Base{}
    {
        const auto capabilities = physical_device.getSurfaceCapabilitiesKHR( surface );
        const auto present_mode = chooseSwapchainPresentMode( physical_device.getSurfacePresentModesKHR( surface ) );

        m_extent = chooseSwapchainExtent( extent, capabilities );
        m_format = chooseSwapchainSurfaceFormat( physical_device.getSurfaceFormatsKHR( surface ) );

        m_min_image_count = std::max( capabilities.maxImageCount, capabilities.minImageCount + 1 );

        auto create_info = vk::SwapchainCreateInfoKHR{
            .surface = surface,
            .minImageCount = m_min_image_count,
            .imageFormat = m_format.format,
            .imageColorSpace = m_format.colorSpace,
            .imageExtent = m_extent,
            .imageArrayLayers = 1,
            .imageUsage = vk::ImageUsageFlagBits::eColorAttachment,
            .preTransform = capabilities.currentTransform,
            .compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque,
            .presentMode = present_mode,
            .clipped = VK_TRUE,
            .oldSwapchain = old_swapchain };

        const auto qfis = std::array{ graphics_queue.familyIndex(), present_queue.familyIndex() };

        if ( graphics_queue.familyIndex() != present_queue.familyIndex() )
        {
            create_info.imageSharingMode = vk::SharingMode::eConcurrent;
            create_info.queueFamilyIndexCount = qfis.size();
            create_info.pQueueFamilyIndices = qfis.data();
        }

        else
        {
            create_info.imageSharingMode = vk::SharingMode::eExclusive;
        }

        auto handle = logical_device.createSwapchainKHRUnique( create_info );
        m_images = logical_device.getSwapchainImagesKHR( *handle );

        auto image_view_create_info = vk::ImageViewCreateInfo{
            .viewType = vk::ImageViewType::e2D,
            .format = create_info.imageFormat,
            .components = {},
            .subresourceRange = k_subrange_info };

        for ( auto& image : m_images )
        {
            image_view_create_info.image = image;
            m_views.push_back( logical_device.createImageViewUnique( image_view_create_info ) );
        }

        static_cast<Base&>( *this ) = std::move( handle );
    }

    using Base::get;
    using Base::operator*;
    using Base::operator->;
    using Base::operator bool;
    using Base::reset;

  public:
    auto& images() const& { return m_images; }
    auto& views() const& { return m_views; }

    vk::SurfaceFormatKHR format() const { return m_format; }
    vk::Extent2D extent() const { return m_extent; }

    auto minImageCount() const { return m_min_image_count; }

  private:
    vk::SurfaceFormatKHR m_format;
    vk::Extent2D m_extent;

    std::vector<vk::Image> m_images;
    std::vector<vk::UniqueImageView> m_views;

    uint32_t m_min_image_count;
};

} // namespace vkwrap