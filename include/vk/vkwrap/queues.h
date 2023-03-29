#pragma once

#include "common/utils.h"
#include "common/vulkan_include.h"

namespace vkwrap
{

class Queue : private vk::Queue
{
  private:
  public:
    const vk::Queue& get() const& { return *this; }
    using vk::Queue::submit;
    using vk::Queue::operator bool;
    using vk::Queue::presentKHR;
    using vk::Queue::waitIdle;
};

} // namespace vkwrap