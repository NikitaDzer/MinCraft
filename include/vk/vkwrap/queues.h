#pragma once

#include "common/utils.h"
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
}

class Queue : private vk::Queue
{
  private:
    QueueFamilyIndex m_family_index = 0;
    QueueIndex m_queue_index = 0;

  public:
    Queue() = default;

    Queue( const vk::Queue& queue, QueueFamilyIndex family_index, QueueIndex queue_index )
        : vk::Queue{ queue },
          m_family_index{ family_index },
          m_queue_index{ queue_index }
    {
    }

    const vk::Queue& get() const& { return *this; }
    QueueFamilyIndex familyIndex() const { return m_family_index; }
    QueueIndex queueIndex() const { return m_queue_index; }

    using vk::Queue::submit;
    using vk::Queue::operator bool;
    using vk::Queue::presentKHR;
    using vk::Queue::waitIdle;

    friend bool operator==( const Queue& lhs, const Queue& rhs )
    {
        return ( lhs.m_family_index == rhs.m_family_index ) && ( lhs.m_queue_index == rhs.m_queue_index ) &&
            ( lhs.get() == rhs.get() );
    }

    friend bool operator!=( const Queue& lhs, const Queue& rhs ) { return !( lhs == rhs ); }
};

} // namespace vkwrap