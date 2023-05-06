#pragma once

#include "common/vulkan_include.h"

#include "vkwrap/queues.h"

namespace vkwrap
{

class CommandPool : private vk::UniqueCommandPool
{
  private:
    using Base = vk::UniqueCommandPool;

  public:
    CommandPool( vk::Device device, vk::UniqueCommandPool pool )
        : Base{ std::move( pool ) },
          m_device{ device }
    {
    } // CommandPool

  private:
    static vk::UniqueCommandPool createPool(
        vk::Device device,
        vk::CommandPoolCreateFlags flags,
        vkwrap::QueueFamilyIndex family_index )
    {
        auto create_info = vk::CommandPoolCreateInfo{ .flags = flags, .queueFamilyIndex = family_index };
        return device.createCommandPoolUnique( create_info );
    } // createPool

  public:
    CommandPool( vk::Device device, vkwrap::Queue queue, vk::CommandPoolCreateFlags flags = {} )
        : Base{ createPool( device, flags, queue.familyIndex() ) },
          m_device{ device }
    {
    } // CommandPool

    using Base::operator->;
    using Base::operator*;
    using Base::get;

    auto createCmdBuffers( uint32_t count, vk::CommandBufferLevel level = vk::CommandBufferLevel::ePrimary )
    {
        auto allocate_info =
            vk::CommandBufferAllocateInfo{ .commandPool = get(), .level = level, .commandBufferCount = count };

        return m_device.allocateCommandBuffersUnique( allocate_info );
    } // createCmdBuffers

    vk::UniqueCommandBuffer createCmdBuffer( vk::CommandBufferLevel level = vk::CommandBufferLevel::ePrimary )
    {
        auto buffers = createCmdBuffers( 1, level );
        return std::move( buffers.at( 0 ) );
    } // createCmdBuffer

  private:
    vk::Device m_device;
};

class OneTimeCommand : private vk::UniqueCommandBuffer
{
  private:
    using Base = vk::UniqueCommandBuffer;

  private:
    using Base::get;

    void begin()
    {
        auto begin_info = vk::CommandBufferBeginInfo{ .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit };
        get().begin( begin_info );
    } // begin

    void end() { get().end(); }

    void submit()
    {
        auto submit_info = vk::SubmitInfo{ .commandBufferCount = 1, .pCommandBuffers = &get() };
        m_queue.submit( submit_info );
    } // submit

    void wait() { m_queue.waitIdle(); }

  public:
    OneTimeCommand( vk::UniqueCommandBuffer unique_cmd, vk::Queue queue )
        : Base{ std::move( unique_cmd ) },
          m_queue{ queue }
    {
    } // oneTimeCommand

    OneTimeCommand( vkwrap::CommandPool& command_pool, vk::Queue queue )
        : Base{ command_pool.createCmdBuffer() },
          m_queue{ queue }
    {
    } // oneTimeCommand

  public:
    template <std::invocable<vk::CommandBuffer&> Callable> void submitAndWait( Callable func )
    {
        begin();
        func( Base::get() );
        end();
        submit();
        wait();
    } // submitAndWait

  private:
    vk::Queue m_queue;
}; // class OneTimeCommand

}; // namespace vkwrap