#pragma once

#include "common/vulkan_include.h"
#include <vk_mem_alloc.h>

#include "utils/misc.h"
#include "vkwrap/core.h"
#include "vkwrap/error.h"

#include <range/v3/algorithm/any_of.hpp>

#include <range/v3/range/conversion.hpp>

#include <array>
#include <bit>
#include <memory>
#include <unordered_map>
#include <utility>

namespace std
{

template <> struct hash<vk::Buffer>
{
    size_t operator()( const vk::Buffer& buffer ) const
    {
        return std::bit_cast<size_t>( buffer.operator VkBuffer() );
    } // operator()

}; // struct hash<vk::Buffer>

template <> struct hash<vk::Image>
{
    size_t operator()( const vk::Image& image ) const
    {
        return std::bit_cast<size_t>( image.operator VkImage() );
    } // operator()

}; // struct hash<vk::Image>

} // namespace std

namespace vkwrap
{

struct AccessMasks
{
    vk::AccessFlags src;
    vk::AccessFlags dst;
}; // struct AccessMasks

struct PipelineStages
{
    vk::PipelineStageFlags src;
    vk::PipelineStageFlags dst;
}; // struct PipelineStages

inline bool
hasStencil( vk::Format format )
{
    using vk::Format;

    constexpr auto k_formats_with_stencil = std::array{
        Format::eS8Uint,
        Format::eD16UnormS8Uint,
        Format::eD24UnormS8Uint,
        Format::eD32SfloatS8Uint,
    };

    return ranges::any_of( k_formats_with_stencil, [ format ]( Format format_with_stencil ) {
        return format == format_with_stencil;
    } );
} // hasStencil

inline vk::ImageAspectFlags
chooseAspectMask( vk::Format format, vk::ImageLayout new_layout )
{
    using vk::ImageAspectFlagBits;
    using vk::ImageAspectFlags;
    using vk::ImageLayout;

    if ( new_layout != ImageLayout::eDepthStencilAttachmentOptimal )
    {
        return ImageAspectFlagBits::eColor;
    }

    ImageAspectFlags aspect_mask{ ImageAspectFlagBits::eDepth };

    if ( hasStencil( format ) )
    {
        aspect_mask |= ImageAspectFlagBits::eStencil;
    }

    return aspect_mask;
} // chooseAspectMask

inline AccessMasks
chooseAccessMasks( vk::ImageLayout old_layout, vk::ImageLayout new_layout )
{
    using vk::AccessFlagBits;
    using vk::AccessFlags;
    using vk::ImageLayout;

    AccessFlags src{ AccessFlagBits::eNone };
    AccessFlags dst{ AccessFlagBits::eNone };

    if ( old_layout == ImageLayout::eUndefined && new_layout == ImageLayout::eTransferDstOptimal )
    {
        src = AccessFlagBits::eNone;
        dst = AccessFlagBits::eTransferWrite;
    } else if ( old_layout == ImageLayout::eTransferDstOptimal && new_layout == ImageLayout::eShaderReadOnlyOptimal )
    {
        src = AccessFlagBits::eTransferWrite;
        dst = AccessFlagBits::eShaderRead;
    } else if ( old_layout == ImageLayout::eUndefined && new_layout == ImageLayout::eDepthStencilAttachmentOptimal )
    {
        src = AccessFlagBits::eNone;
        dst = AccessFlagBits::eDepthStencilAttachmentRead | AccessFlagBits::eDepthStencilAttachmentWrite;
    } else
    {
        throw Error{ "chooseAccessMasks: unimplemented layout transition." };
    }

    return { src, dst };
} // chooseAccessMasks

inline vk::ImageMemoryBarrier
createImageBarrierInfo( vk::Image& image, vk::Format format, vk::ImageLayout old_layout, vk::ImageLayout new_layout )
{
    using vk::AccessFlagBits;
    using vk::ImageMemoryBarrier;

    auto [ src_access, dst_access ] = chooseAccessMasks( old_layout, new_layout );

    // clang-format off
    ImageMemoryBarrier barrier{
        .srcAccessMask = src_access,
        .dstAccessMask = dst_access,

        .oldLayout = old_layout,
        .newLayout = new_layout,

        /** Any combination of image::sharingMode,
         * barrier::srcQueueFamilyIndex, barrier::dstQueueFamilyIndex is allowed.
         */
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,

        .image = image,

        .subresourceRange = {
            .aspectMask = chooseAspectMask( format, new_layout ),

            // We use images with one layer and one mipmap.
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        }
    };
    // clang-format on

    return barrier;
} // createImageBarrierInfo

inline PipelineStages
choosePipelineStages( vk::ImageLayout old_layout, vk::ImageLayout new_layout )
{
    using vk::ImageLayout;
    using vk::PipelineStageFlagBits;
    using vk::PipelineStageFlags;

    PipelineStageFlags src{ PipelineStageFlagBits::eNone };
    PipelineStageFlags dst{ PipelineStageFlagBits::eNone };

    if ( old_layout == ImageLayout::eUndefined && new_layout == ImageLayout::eTransferDstOptimal )
    {
        src = PipelineStageFlagBits::eTopOfPipe;
        dst = PipelineStageFlagBits::eTransfer;
    } else if ( old_layout == ImageLayout::eTransferDstOptimal && new_layout == ImageLayout::eShaderReadOnlyOptimal )
    {
        src = PipelineStageFlagBits::eTransfer;
        dst = PipelineStageFlagBits::eFragmentShader;
    } else if ( old_layout == ImageLayout::eUndefined && new_layout == ImageLayout::eDepthStencilAttachmentOptimal )
    {
        src = PipelineStageFlagBits::eTopOfPipe;
        dst = PipelineStageFlagBits::eEarlyFragmentTests;
    } else
    {
        throw Error{ "choosePipelineStages: unimplemented layout transition." };
    }

    return { src, dst };
} // choosePipelineStages

class Mman
{

  private:
    struct BufferInfo
    {
        VmaAllocation allocation;
        vk::DeviceSize size;
    }; // struct BufferInfo

    struct ImageInfo
    {
        VmaAllocation allocation;
        vk::Format format;
        vk::ImageLayout layout;
        vk::Extent3D extent;
    }; // struct ImageInfo

    class OneTimeCommand : private vk::UniqueCommandBuffer
    {

      private:
        using Base = vk::UniqueCommandBuffer;

      private:
        vk::Queue m_queue;

        void begin() const
        {
            // clang-format off
            vk::CommandBufferBeginInfo begin_info{ 
                .flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit };
            // clang-format on

            Base::get().begin( begin_info );
        } // begin

        void end() const { Base::get().end(); }

        void submit() const
        {
            vk::SubmitInfo submit_info{ .commandBufferCount = 1, .pCommandBuffers = &Base::get() };

            m_queue.submit( std::array{ submit_info } );
        } // submit

        void wait() const { m_queue.waitIdle(); }

      public:
        OneTimeCommand( vk::UniqueCommandBuffer unique_cmd, vk::Queue queue )
            : Base{ std::move( unique_cmd ) },
              m_queue{ queue }
        {
        } // oneTimeCommand

        void submitAndWait( std::function<void( vk::CommandBuffer& )> func )
        {
            begin();
            func( Base::get() );
            end();
            submit();
            wait();
        } // submitAndWait

        using Base::operator->;
    }; // class OneTimeCommand

  private:
    VmaAllocator m_vma;
    vk::Queue m_queue;
    OneTimeCommand m_cmd;

    std::unordered_map<vk::Buffer, BufferInfo> m_buffers_info;
    std::unordered_map<vk::Image, ImageInfo> m_images_info;

    BufferInfo* findInfo( vk::Buffer buffer )
    {
        if ( auto entry = m_buffers_info.find( buffer ); entry == m_buffers_info.end() )
        {
            throw Error{ "Mman: buffer info not found." };
        } else
        {
            return &entry->second;
        }
    } // findInfo

    ImageInfo* findInfo( vk::Image image )
    {
        if ( auto entry = m_images_info.find( image ); entry == m_images_info.end() )
        {
            throw Error{ "Mman: image info not found." };
        } else
        {
            return &entry->second;
        }
    } // findInfo

    VmaAllocation getAllocation( auto target ) { return findInfo( target )->allocation; }

    void addInfo( vk::Buffer buffer, const BufferInfo& info ) { m_buffers_info.insert( { buffer, info } ); }
    void addInfo( vk::Image image, const ImageInfo& info ) { m_images_info.insert( { image, info } ); }

    void clearInfo( vk::Buffer buffer ) { m_buffers_info.erase( buffer ); }
    void clearInfo( vk::Image image ) { m_images_info.erase( image ); }

    VmaDetailedStatistics getTotalStats() const
    {
        VmaTotalStatistics stats{};
        vmaCalculateStatistics( m_vma, &stats );

        return stats.total;
    } // getTotalStats

    OneTimeCommand& getCommand() { return m_cmd; }

    static constexpr VmaAllocationCreateInfo k_buffer_alloc_create_info{
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO };

    static constexpr VmaAllocationCreateInfo k_image_alloc_create_info{
        .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT };

    static constexpr VmaVulkanFunctions k_vulkan_functions = {
        .vkGetInstanceProcAddr = &vkGetInstanceProcAddr,
        .vkGetDeviceProcAddr = &vkGetDeviceProcAddr };

    static OneTimeCommand createCommand( vk::Device device, vk::CommandPool cmd_pool, vk::Queue queue )
    {
        vk::CommandBufferAllocateInfo alloc_info{
            .commandPool = cmd_pool,
            .level = vk::CommandBufferLevel::ePrimary,
            .commandBufferCount = 1 };

        // clang-format off
        vk::UniqueCommandBuffer cmd_buffer{ 
            std::move( device.allocateCommandBuffersUnique( alloc_info )[ 0 ] ) };
        // clang-format on

        return { std::move( cmd_buffer ), queue };
    } // createCommandBuffer

  public:
    Mman(
        vkwrap::VulkanVersion version,
        vk::Instance instance,
        vk::PhysicalDevice physical_device,
        vk::Device logical_device,
        vk::Queue queue,
        vk::CommandPool cmd_pool )
        : m_vma{ VK_NULL_HANDLE },
          m_queue{ queue },
          m_cmd{ createCommand( logical_device, cmd_pool, queue ) }
    {
        VmaAllocatorCreateInfo create_info{
            .physicalDevice = physical_device,
            .device = logical_device,
            .pVulkanFunctions = &k_vulkan_functions,
            .instance = instance,
            .vulkanApiVersion = utils::toUnderlying( version ) };

        VkResult result = vmaCreateAllocator( &create_info, &m_vma );
        vk::resultCheck( vk::Result{ result }, "Mman: allocator creation error." );
    } // Mman

    ~Mman() { vmaDestroyAllocator( m_vma ); }

    Mman( const Mman& ) = delete;
    Mman( Mman&& ) = delete;
    Mman& operator=( const Mman& ) = delete;
    Mman& operator=( Mman&& ) = delete;

    vk::Buffer create( const vk::BufferCreateInfo& create_info )
    {
        VkBuffer buffer{};
        VmaAllocation allocation{};

        VkResult result = vmaCreateBuffer(
            m_vma,
            &static_cast<const VkBufferCreateInfo&>( create_info ),
            &k_buffer_alloc_create_info,
            &buffer,
            &allocation,
            nullptr );

        vk::resultCheck( vk::Result{ result }, "Mman: buffer allocation error." );

        BufferInfo buffer_info{ allocation, create_info.size };
        addInfo( buffer, buffer_info );

        return buffer;
    } // create

    vk::Image create( const vk::ImageCreateInfo& create_info )
    {
        VkImage image{};
        VmaAllocation allocation{};

        VkResult result = vmaCreateImage(
            m_vma,
            &static_cast<const VkImageCreateInfo&>( create_info ),
            &k_image_alloc_create_info,
            &image,
            &allocation,
            nullptr );

        vk::resultCheck( vk::Result{ result }, "Mman: image allocation error." );

        ImageInfo image_info{ allocation, create_info.format, create_info.initialLayout, create_info.extent };
        addInfo( image, image_info );

        return image;
    } // create

    void destroy( vk::Buffer buffer )
    {
        vmaDestroyBuffer( m_vma, buffer, getAllocation( buffer ) );
        clearInfo( buffer );
    } // destroy

    void destroy( vk::Image image )
    {
        vmaDestroyImage( m_vma, image, getAllocation( image ) );
        clearInfo( image );
    } // destroy

    uint8_t* map( vk::Buffer buffer )
    {
        uint8_t* mapped = nullptr;

        VkResult result = vmaMapMemory( m_vma, getAllocation( buffer ), reinterpret_cast<void**>( &mapped ) );
        vk::resultCheck( vk::Result{ result }, "Mman: buffer mapping error." );

        return mapped;
    } // map

    void unmap( vk::Buffer buffer ) { vmaUnmapMemory( m_vma, getAllocation( buffer ) ); }

    void flush( vk::Buffer buffer )
    {
        VkResult result = vmaFlushAllocation( m_vma, getAllocation( buffer ), 0, VK_WHOLE_SIZE );
        vk::resultCheck( vk::Result{ result }, "Mman: buffer flushing error." );
    } // flush

    void copy(
        vk::Buffer src_buffer,
        vk::Buffer dst_buffer,
        vk::DeviceSize src_offset,
        vk::DeviceSize dst_offset,
        vk::DeviceSize size )
    {
        vk::BufferCopy region{ src_offset, dst_offset, size };

        // clang-format off
        getCommand().submitAndWait( [ & ]( auto& cmd ) { 
            cmd.copyBuffer( src_buffer, dst_buffer, std::array{ region } ); 
        });
        // clang-format on
    } // copy

    void copy( vk::Buffer src_buffer, vk::Buffer dst_buffer )
    {
        copy( src_buffer, dst_buffer, 0, 0, findInfo( src_buffer )->size );
    } // copy

    void copy( vk::Buffer src_buffer, vk::Image dst_image )
    {
        // TODO: add interface to configure image and buffer params.

        ImageInfo* image_info = findInfo( dst_image );
        vk::BufferImageCopy region{
            .bufferOffset = 0,
            .bufferRowLength = 0,
            .bufferImageHeight = 0,

            .imageSubresource =
                { .aspectMask = vk::ImageAspectFlagBits::eColor,

                  // We use images with one layer and one mipmap.
                  .mipLevel = 0,
                  .baseArrayLayer = 0,
                  .layerCount = 1 },

            .imageOffset = vk::Offset3D{ 0, 0, 0 },
            .imageExtent =
                vk::Extent3D{
                    image_info->extent.width,
                    image_info->extent.height,
                    1 /* depth */
                },
        };

        getCommand().submitAndWait( [ & ]( auto& cmd ) {
            cmd.copyBufferToImage( src_buffer, dst_image, image_info->layout, std::array{ region } );
        } );
    } // copy

    void transit( vk::Image image, vk::ImageLayout new_layout )
    {
        ImageInfo* image_info = findInfo( image );

        auto [ src_stage, dst_stage ] = choosePipelineStages( image_info->layout, new_layout );
        vk::ImageMemoryBarrier barrier{
            createImageBarrierInfo( image, image_info->format, image_info->layout, new_layout ) };

        getCommand().submitAndWait( [ src_stage = src_stage, dst_stage = dst_stage, &barrier ]( auto& cmd ) {
            cmd.pipelineBarrier(
                src_stage,
                dst_stage,
                vk::DependencyFlagBits::eByRegion, /** TODO: learn more about dependency.
                                                    * Probably, it may work incorrect.
                                                    */
                std::array<vk::MemoryBarrier, 0>{},
                std::array<vk::BufferMemoryBarrier, 0>{},
                std::array{ barrier } );
        } );

        image_info->layout = new_layout;
    } // transit

    vk::DeviceSize getAllocatedBytes() { return getTotalStats().statistics.blockBytes; }

    vk::DeviceSize getUsedBytes() { return getTotalStats().statistics.allocationBytes; }

    uint64_t getAllocatedBytesMB()
    {
        constexpr uint64_t bytes_in_mb = 1024 * 1024;
        return getAllocatedBytes() / bytes_in_mb;
    } // getAllocatedBytesMB

    uint64_t getUsedBytesMB()
    {
        constexpr uint64_t bytes_in_mb = 1024 * 1024;
        return getUsedBytes() / bytes_in_mb;
    } // getUsedBytesMB

}; // class Mman

} // namespace vkwrap
