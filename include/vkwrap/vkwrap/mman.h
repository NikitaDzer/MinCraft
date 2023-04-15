#pragma once

#include "common/vulkan_include.h"

#include "vkwrap/core.h"
#include "vkwrap/error.h"

#include "vk_mem_alloc.h"

#include <memory>
#include <array>
#include <utility>
#include <unordered_map>

#include <range/v3/view/set_algorithm.hpp>
#include <range/v3/view/take.hpp>
#include <range/v3/view/transform.hpp>

#include <range/v3/algorithm/copy.hpp>
#include <range/v3/algorithm/set_algorithm.hpp>
#include <range/v3/algorithm/sort.hpp>
#include <range/v3/algorithm/any_of.hpp>

#include "range/v3/range/conversion.hpp"
#include <range/v3/iterator/insert_iterators.hpp>
#include <range/v3/range/concepts.hpp>

#include <range/v3/view/all.hpp>
#include <range/v3/view/filter.hpp>
#include <range/v3/view/iota.hpp>
#include <range/v3/view/zip.hpp>
#include <range/v3/view/zip_with.hpp>


namespace std
{

template <> struct hash<vk::Buffer>
{
    size_t operator()( const vk::Buffer& buffer ) const
    {
        return reinterpret_cast<size_t>( buffer.operator VkBuffer() );
    }
}; 

template <> struct hash<vk::Image>
{
    size_t operator()( const vk::Image& image ) const
    {
        return reinterpret_cast<size_t>( image.operator VkImage() );
    }
}; 

}

namespace vkwrap
{

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

    auto isEqualFormats = [ format ]( Format format_with_stencil )
    {
        return format == format_with_stencil;
    };

    return ranges::any_of( k_formats_with_stencil, isEqualFormats );
}

inline vk::ImageAspectFlags
chooseAspectMask( vk::Format format, vk::ImageLayout new_layout )
{
    using vk::ImageLayout;
    using vk::ImageAspectFlags;
    using vk::ImageAspectFlagBits;

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
}

inline std::pair<vk::AccessFlags, vk::AccessFlags>
chooseAccessMasks( vk::ImageLayout old_layout, vk::ImageLayout new_layout )
{
    using vk::AccessFlags;
    using vk::AccessFlagBits;
    using vk::ImageLayout;

    AccessFlags src{ AccessFlagBits::eNone };
    AccessFlags dst{ AccessFlagBits::eNone };

    if ( old_layout == ImageLayout::eUndefined 
         && new_layout == ImageLayout::eTransferDstOptimal )
    {
        src = AccessFlagBits::eNone;
        dst = AccessFlagBits::eTransferWrite;
    } else if ( old_layout == ImageLayout::eTransferDstOptimal 
                && new_layout == ImageLayout::eShaderReadOnlyOptimal )
    {
        src = AccessFlagBits::eTransferWrite;
        dst = AccessFlagBits::eShaderRead;
    } else if ( old_layout == ImageLayout::eUndefined 
                && new_layout == ImageLayout::eDepthStencilAttachmentOptimal)
    {
        src = AccessFlagBits::eNone;
        dst = AccessFlagBits::eDepthStencilAttachmentRead 
              | AccessFlagBits::eDepthStencilAttachmentWrite;
    } else
    {
        throw Error{ "chooseAccessMasks: unimplemented layout transition." };
    }

    return { src, dst };
}

inline vk::ImageMemoryBarrier 
createImageBarrierInfo( vk::Image& image, vk::Format format, vk::ImageLayout old_layout, vk::ImageLayout new_layout )
{
    using vk::ImageMemoryBarrier;
    using vk::AccessFlagBits;

    ImageMemoryBarrier barrier{};

    barrier.image = image;

    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;

    /** Any combination of image::sharingMode, 
      * barrier::srcQueueFamilyIndex, barrier::dstQueueFamilyIndex is allowed.
      */
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

    try 
    {
        auto [ src_access, dst_access ] = chooseAccessMasks( old_layout, new_layout );
        barrier.srcAccessMask = src_access;
        barrier.dstAccessMask = dst_access;
    } catch ( Error& e )
    {
        throw;
    }

    // We use images with one layer and one mipmap.
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    barrier.subresourceRange.aspectMask = chooseAspectMask( format, new_layout );

    return barrier;
}

inline std::pair<vk::PipelineStageFlags, vk::PipelineStageFlags>
choosePipelineStages( vk::ImageLayout old_layout, vk::ImageLayout new_layout )
{
    using vk::PipelineStageFlags;
    using vk::PipelineStageFlagBits;
    using vk::ImageLayout;

    PipelineStageFlags src{ PipelineStageFlagBits::eNone };
    PipelineStageFlags dst{ PipelineStageFlagBits::eNone };

    if ( old_layout == ImageLayout::eUndefined 
         && new_layout == ImageLayout::eTransferDstOptimal )
    {
        src = PipelineStageFlagBits::eTopOfPipe;
        dst = PipelineStageFlagBits::eTransfer;
    } else if ( old_layout == ImageLayout::eTransferDstOptimal 
                && new_layout == ImageLayout::eShaderReadOnlyOptimal )
    {
        src = PipelineStageFlagBits::eTransfer;
        dst = PipelineStageFlagBits::eFragmentShader;
    } else if ( old_layout == ImageLayout::eUndefined 
                && new_layout == ImageLayout::eDepthStencilAttachmentOptimal)
    {
        src = PipelineStageFlagBits::eTopOfPipe;
        dst = PipelineStageFlagBits::eEarlyFragmentTests;
    } else
    {
        throw Error{ "choosePipelineStages: unimplemented layout transition." };
    }

    return { src, dst };
}

class Mman
{

private:
    struct BufferInfo
    {
        VmaAllocation allocation;
        vk::DeviceSize size;
    };

    struct ImageInfo
    {
        VmaAllocation allocation;
        vk::Format format;
        vk::ImageLayout layout;
        vk::Extent3D extent;
    };

    class SingleAction : private vk::UniqueCommandBuffer
    {
    
    private:
        using Base = vk::UniqueCommandBuffer;

    private:
        vk::Queue m_queue;

        vk::UniqueCommandBuffer createCommandBuffer( vk::Device device, vk::CommandPool cmd_pool )
        {
            vk::CommandBufferAllocateInfo alloc_info{};
            alloc_info.commandPool = cmd_pool;
            alloc_info.level = vk::CommandBufferLevel::ePrimary;
            alloc_info.commandBufferCount = 1;

            return std::move( device.allocateCommandBuffersUnique( alloc_info )[ 0 ] );
        }

        void begin()
        {
            vk::CommandBufferBeginInfo begin_info{};
            begin_info.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;

            Base::get().begin( begin_info );
        }

        void end()
        {
            Base::get().end();
        }

        void submitAndWait()
        {
            vk::SubmitInfo submit_info{};
            submit_info.commandBufferCount = 1;
            submit_info.pCommandBuffers = &Base::get();

            m_queue.submit( std::array{ submit_info } );
            m_queue.waitIdle();
        }
        
    public:
        SingleAction( vk::Device device, vk::CommandPool cmd_pool, vk::Queue queue ):
            Base{ createCommandBuffer( device, cmd_pool ) },
            m_queue{ queue }
        {
            begin(); 
        }

        SingleAction( const SingleAction& ) = delete;
        SingleAction& operator = ( const SingleAction& ) = delete;

        SingleAction( SingleAction&& action ):
            Base{ std::move( action ) },
            m_queue{ std::move( action.m_queue ) }
        {
        }

        SingleAction& operator = ( SingleAction&& action )
        {
            std::swap( *this, action );
            return *this;
        }

        ~SingleAction()
        {
            end();
            submitAndWait();
        }

        using Base::operator ->;
    };

private:
    VmaAllocator m_vma;
    vk::Device m_device;
    vk::Queue m_queue;
    vk::CommandPool m_cmd_pool;

    std::unordered_map<vk::Buffer, BufferInfo> m_buffers_info;
    std::unordered_map<vk::Image, ImageInfo> m_images_info;

    BufferInfo* findInfo( vk::Buffer buffer )
    {
        if ( auto entry = m_buffers_info.find( buffer );
             entry == m_buffers_info.end() )
        {
            throw Error{ "Mman: buffer info not found." };    
        } else
        {
            return &entry->second;
        }
    }

    ImageInfo* findInfo( vk::Image image )
    {
        if ( auto entry = m_images_info.find( image );
             entry == m_images_info.end() )
        {
            throw Error{ "Mman: image info not found." };
        } else
        {
            return &entry->second;
        }
    }

    VmaAllocation* findAllocation( auto target )
    {
        return &findInfo( target )->allocation; 
    }

    void addInfo( vk::Buffer buffer, const BufferInfo& info )
    {
        m_buffers_info.insert({ buffer, info });
    }

    void addInfo( vk::Image image, const ImageInfo& info )
    {
        m_images_info.insert({ image, info });
    }

    SingleAction createAction() const
    {
        return { m_device, m_cmd_pool, m_queue };
    }

    VmaDetailedStatistics getTotalStats() const
    {
        VmaTotalStatistics stats{};
        vmaCalculateStatistics( m_vma, &stats );

        return stats.total;
    }

    static constexpr VmaAllocationCreateInfo k_buffer_alloc_create_info = {
        .flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT,
        .usage = VMA_MEMORY_USAGE_AUTO
    };

    static constexpr VmaAllocationCreateInfo k_image_alloc_create_info = {
        .usage = VMA_MEMORY_USAGE_AUTO,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    };

    static constexpr VmaVulkanFunctions k_vulkan_functions = {
        .vkGetInstanceProcAddr = &vkGetInstanceProcAddr,
        .vkGetDeviceProcAddr = &vkGetDeviceProcAddr
    };

public:
    Mman( vkwrap::VulkanVersion version, vk::Instance instance, 
          vk::PhysicalDevice physical_device, vk::Device logical_device, 
          vk::Queue queue, vk::CommandPool cmd_pool ):
        m_vma{ VK_NULL_HANDLE },
        m_device{ logical_device },
        m_queue{ queue },
        m_cmd_pool{ cmd_pool }
    {

        // FIX: version setting.
        VmaAllocatorCreateInfo create_info{};
        create_info.vulkanApiVersion = static_cast<uint32_t>( version );
        create_info.instance = instance;
        create_info.physicalDevice = physical_device;
        create_info.device = logical_device;
        create_info.pVulkanFunctions = &k_vulkan_functions;

        VkResult result = vmaCreateAllocator( &create_info, &m_vma );
        if ( result != VK_SUCCESS )
        {
            throw Error{ "Mman: allocator creation error." };
        }
    }

    ~Mman()
    {
        vmaDestroyAllocator( m_vma );
    }

    Mman( const Mman& ) = delete;
    Mman( Mman&& ) = delete;
    Mman& operator = ( const Mman& ) = delete;
    Mman& operator = ( Mman&& ) = delete;

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
            nullptr 
        );

        if ( result != VK_SUCCESS )
        {
            throw Error{ "Mman: buffer allocation error." };
        }

        vk::Buffer buffer_hpp{ buffer };
        BufferInfo buffer_info{
            allocation,
            create_info.size
        };

        addInfo( buffer_hpp, buffer_info );

        return buffer_hpp;
    }

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
            nullptr
        );

        if ( result != VK_SUCCESS )
        {
            throw Error{ "Mman: image allocation error." };
        }

        vk::Image image_hpp{ image };
        ImageInfo image_info{ 
            allocation,
            create_info.format,
            create_info.initialLayout,
            create_info.extent 
        };

        addInfo( image_hpp, image_info );

        return image_hpp;
    }

    void destroy( vk::Buffer buffer )
    {
        vmaDestroyBuffer( m_vma, buffer, *findAllocation( buffer ) );
    }

    void destroy( vk::Image image )
    {
        vmaDestroyImage( m_vma, image, *findAllocation( image ) );
    }

    uint8_t* map( vk::Buffer buffer )
    {
        uint8_t* mapped = nullptr;

        VkResult result = vmaMapMemory( m_vma, *findAllocation( buffer ), reinterpret_cast<void **>( &mapped ) );
        if ( result != VK_SUCCESS )
        {
            throw Error{ "Mman: buffer mapping error." };
        }

        return mapped;
    }

    void unmap( vk::Buffer buffer )
    {
        vmaUnmapMemory( m_vma, *findAllocation( buffer ) );
    }

    void flush( vk::Buffer buffer )
    {
        VkResult result = vmaFlushAllocation( m_vma, *findAllocation( buffer ), 0, VK_WHOLE_SIZE );
        
        if ( result != VK_SUCCESS )
        {
            throw Error{ "Mman: buffer flushing error." };
        }
    }

    void copy( vk::Buffer src_buffer, vk::Buffer dst_buffer, 
               vk::DeviceSize src_offset, vk::DeviceSize dst_offset, 
               vk::DeviceSize size )
    {
        vk::BufferCopy region{ src_offset, dst_offset, size };

        createAction()->copyBuffer(
            src_buffer, 
            dst_buffer, 
            std::array{ region }
        );
    }

    void copy( vk::Buffer src_buffer, vk::Buffer dst_buffer )
    {
        copy( src_buffer, dst_buffer, 0, 0, findInfo( src_buffer )->size );
    }

    void copy( vk::Buffer src_buffer, vk::Image dst_image )
    {
        // TODO: add interface to configure image and buffer params.

        ImageInfo* image_info = findInfo( dst_image );
        vk::BufferImageCopy region{};

        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;

        region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        
        // We use images with one layer and one mipmap.
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;

        region.imageOffset = vk::Offset3D{ 0, 0, 0 };
        region.imageExtent = vk::Extent3D{
            image_info->extent.width,
            image_info->extent.height,
            1 /* depth */
        };

        createAction()->copyBufferToImage(
            src_buffer,
            dst_image,
            image_info->layout,
            std::array{ region }
        );
    }

    void transit( vk::Image image, vk::ImageLayout new_layout )
    {
        ImageInfo* image_info = findInfo( image );

        vk::ImageMemoryBarrier barrier{};
        vk::PipelineStageFlags src_stage{};
        vk::PipelineStageFlags dst_stage{};

        try
        {
            barrier = createImageBarrierInfo( 
                image,
                image_info->format,
                image_info->layout,
                new_layout
            );

            auto [ src_stage_tmp, dst_stage_tmp ] 
                = choosePipelineStages( image_info->layout, new_layout );
            src_stage = src_stage_tmp;
            dst_stage = dst_stage_tmp;
        } catch ( Error& e )
        {
            throw Error{ std::string{ "Mman: " } + e.what() };
        }

        createAction()->pipelineBarrier( 
            src_stage, 
            dst_stage,
            vk::DependencyFlagBits::eByRegion, /** TODO: learn more about dependency.
                                                 * Probably, it may work incorrect.
                                                 */
            0, nullptr,
            0, nullptr,
            1, &barrier
        );

        image_info->layout = new_layout;
    }

    vk::DeviceSize getAllocatedBytes()
    {
        return getTotalStats().statistics.blockBytes;
    }

    vk::DeviceSize getUsedBytes()
    {
        return getTotalStats().statistics.allocationBytes;
    }

    uint64_t getAllocatedBytesMB()
    {
        constexpr uint64_t bytes_in_mb = 1024 * 1024;
        return getAllocatedBytes() / bytes_in_mb;
    }

    uint64_t getUsedBytesMB()
    {
        constexpr uint64_t bytes_in_mb = 1024 * 1024;
        return getUsedBytes() / bytes_in_mb;
    }

};


// TODO: concept for base template class.
template <class Base>
class Allocatable : public Base
{

private:
    Mman* m_mman;

public:
    Allocatable( const Base& base, Mman& mman ):
        Base{ base },
        m_mman{ &mman }
    {
    }

    virtual ~Allocatable()
    {
        m_mman->destroy( *this );
    }

    Allocatable( const Allocatable& ) = delete;
    Allocatable( Allocatable&& ) = delete;

    Allocatable& operator = ( const Allocatable& ) = delete;
    Allocatable& operator = ( Allocatable&& ) = delete;

    Mman* getMman() { return m_mman; }

};


}
