#pragma once

#include "common/vulkan_include.h"

#include "vkwrap/core.h"

namespace vkwrap
{

inline bool
queueFamilySupportsPresent(
    const vk::PhysicalDevice& physical_device,
    const vk::SurfaceKHR& surface,
    QueueFamilyIndex qfi )
{
    return physical_device.getSurfaceSupportKHR( qfi, surface );
} // queueFamilySupportsPresent

class Queue : private vk::Queue
{
  private:
    QueueFamilyIndex m_family_index = 0;
    QueueIndex m_queue_index = 0;

    Queue( const vk::Queue& queue, QueueFamilyIndex family_index, QueueIndex queue_index )
        : vk::Queue{ queue },
          m_family_index{ family_index },
          m_queue_index{ queue_index }
    {
    }

  public:
    Queue() = default;

    Queue( const vk::Device& logical_device, QueueFamilyIndex family_index, QueueIndex queue_index )
        : Queue{ logical_device.getQueue( family_index, queue_index ), family_index, queue_index }
    {
    }

    const vk::Queue& get() const& { return *this; }
    QueueFamilyIndex familyIndex() const { return m_family_index; }
    QueueIndex queueIndex() const { return m_queue_index; }

    using vk::Queue::submit;
    using vk::Queue::operator bool;
    using vk::Queue::presentKHR;
    using vk::Queue::waitIdle;

    bool operator==( const Queue& rhs ) const = default;

    template <typename Dispatch = VULKAN_HPP_DEFAULT_DISPATCHER_TYPE>
    vk::Result presentKHRWithOutOfDate(
        const vk::PresentInfoKHR& present_info,
        const Dispatch& d = VULKAN_HPP_DEFAULT_DISPATCHER ) const
    {
        vk::Result result_present;
        try
        {
            result_present = presentKHR( present_info, d );
        } catch ( vk::OutOfDateKHRError& )
        {
            result_present = vk::Result::eErrorOutOfDateKHR;
        }
        return result_present;
    }

    template <typename Dispatch = VULKAN_HPP_DEFAULT_DISPATCHER_TYPE>
    vk::Result presentKHRWithOutOfDate(
        vk::SwapchainKHR swapchain,
        vk::Semaphore wait_semaphore,
        uint32_t image_index,
        const Dispatch& d = VULKAN_HPP_DEFAULT_DISPATCHER ) const
    {
        auto present_info = vk::PresentInfoKHR{
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &wait_semaphore,
            .swapchainCount = 1,
            .pSwapchains = &swapchain,
            .pImageIndices = &image_index };

        return presentKHRWithOutOfDate( present_info, d );
    }
}; // Queue

} // namespace vkwrap