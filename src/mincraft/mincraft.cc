#include "common/vulkan_include.h"
#include "utils/color.h"

#include "vkwrap/command.h"
#include "vkwrap/device.h"
#include "vkwrap/framebuffer.h"
#include "vkwrap/instance.h"
#include "vkwrap/pipeline.h"
#include "vkwrap/queues.h"

#include "vkwrap/buffer.h"
#include "vkwrap/image.h"
#include "vkwrap/image_view.h"
#include "vkwrap/sampler.h"
#include "vkwrap/swapchain.h"

#include "chunk/chunk_man.h"
#include "chunk/chunk_mesher.h"
#include "glfw/window.h"
#include "gui/gui.h"

#include <spdlog/cfg/env.h>
#include <spdlog/fmt/bundled/core.h>
#include <spdlog/fmt/bundled/format.h>

#include <popl/popl.hpp>

#include "glm_include.hpp"

#include <range/v3/algorithm/any_of.hpp>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/view/iota.hpp>
#include <range/v3/view/single.hpp>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <functional>
#include <iterator>
#include <numeric>
#include <sstream>
#include <string>
#include <utility>

#include <ktx.h>
#include <ktxvulkan.h>

namespace
{

struct UniformBufferObject
{
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;

    glm::vec2 origin_pos;
};

struct DebugCallback
{
    std::atomic<unsigned> call_count = 0;
    bool operator()(
        vk::DebugUtilsMessageSeverityFlagBitsEXT sev,
        vk::DebugUtilsMessageTypeFlagsEXT type,
        const vk::DebugUtilsMessengerCallbackDataEXT& data )
    {
        ++call_count;
        return vkwrap::defaultDebugCallback( sev, type, data );
    }
}; // CountingCallback

constexpr auto k_vulkan_version = vkwrap::VulkanVersion::e_version_1_3;

struct CreateInstanceResult
{
    vkwrap::GenericInstance instance;
    std::shared_ptr<DebugCallback> callback;
};

CreateInstanceResult
createInstance( const glfw::Instance& glfw_instance, bool validation )
{
    auto instance_builder = vkwrap::InstanceBuilder{};
    instance_builder.withVersion( k_vulkan_version ).withExtensions( glfw_instance.getRequiredExtensions() );

    auto make_instance = [ &instance_builder ]() {
        auto instance = instance_builder.make();
        VULKAN_HPP_DEFAULT_DISPATCHER.init( instance.get() );
        return instance;
    };

    if ( validation )
    {
        auto functor = std::make_shared<DebugCallback>();
        auto callback = [ functor ]( auto&& sev, auto&& type, auto&& data ) -> bool // Capture by shared ptr by value
        {
            return functor->operator()( sev, type, data );
        };

        instance_builder.withDebugMessenger().withValidationLayers().withCallback( callback );
        return CreateInstanceResult{ make_instance(), functor };
    }

    return CreateInstanceResult{ make_instance(), nullptr };
}

struct AppOptions
{
    bool validation = false;
};

std::optional<AppOptions>
parseOptions( std::span<const char*> command_line_args )
{
    popl::OptionParser op( "Allowed options" );

    auto help_option = op.add<popl::Switch>( "h", "help", "Print this help message" );
    auto validation_option = op.add<popl::Switch>( "d", "debug", "" );

    op.parse( command_line_args.size(), command_line_args.data() );

    if ( help_option->is_set() )
    {
        fmt::print( "{}", op.help() );
        return std::nullopt;
    }

#ifndef NDEBUG
    bool validation = true; // Enable validation layers in debug build
#else
    bool validation = validation_option->is_set();
#endif

    return AppOptions{ .validation = validation };
}

vkwrap::PhysicalDevice
pickPhysicalDevice( vk::Instance instance, vk::SurfaceKHR surface, vkwrap::SwapchainReqs swapchain_requirements )
{
    vkwrap::PhysicalDeviceSelector physical_selector;

    // Calculate weights depending on the requirements. Should be expanded to check for Format capabilities e.t.c. Help
    // needed :)
    auto weight_functor = [ &swapchain_requirements ]( vkwrap::PhysicalDeviceInfo info ) {
        return swapchain_requirements.calculateWeight( info.device.get() );
    };

    physical_selector.withExtensions( std::array{ VK_KHR_SWAPCHAIN_EXTENSION_NAME } )
        .withTypes( std::array{ vk::PhysicalDeviceType::eDiscreteGpu, vk::PhysicalDeviceType::eIntegratedGpu } )
        .withVersion( vkwrap::VulkanVersion::e_version_1_3 )
        .withWeight( weight_functor );

    auto suitable_devices = physical_selector.make( instance );

    if ( suitable_devices.empty() )
    {
        throw std::runtime_error{ "No suitable physical devices found" };
    }

    return suitable_devices.at( 0 ).info.device;
}

struct LogicalDeviceCreateResult
{
    vkwrap::LogicalDevice device;
    vkwrap::Queue graphics;
    vkwrap::Queue present;
};

LogicalDeviceCreateResult
createLogicalDeviceQueues( vk::PhysicalDevice physical_device, vk::SurfaceKHR surface )
{
    vkwrap::Queue graphics;
    vkwrap::Queue present;
    vkwrap::LogicalDeviceBuilder device_builder;

    device_builder.withExtensions( std::array{ VK_KHR_SWAPCHAIN_EXTENSION_NAME } )
        .withGraphicsQueue( graphics )
        .withPresentQueue( surface, present );

    auto logical_device = device_builder.make( physical_device );
    VULKAN_HPP_DEFAULT_DISPATCHER.init( logical_device.get() );

    return LogicalDeviceCreateResult{ .device = std::move( logical_device ), .graphics = graphics, .present = present };
}

vk::Extent2D
getWindowExtent( const glfw::wnd::Window& window )
{
    auto framebuffer_size = window.getFramebufferSize();
    return vk::Extent2D{
        .width = static_cast<uint32_t>( framebuffer_size.width ),
        .height = static_cast<uint32_t>( framebuffer_size.height ) };
}

using Framebuffers = std::vector<vkwrap::Framebuffer>;

struct SwapchainRecreateResult
{
    vkwrap::Swapchain swapchain;
    Framebuffers framebuffers;
};

Framebuffers
createFramebuffers(
    vkwrap::Swapchain& swapchain,
    vk::ImageView depth_image_view,
    vk::Device logical_device,
    vk::RenderPass render_pass )
{
    auto extent = swapchain.getExtent();
    Framebuffers framebuffers;

    auto image_views = ranges::views::iota( uint32_t{ 0 }, swapchain.getImagesCount() ) |
        ranges::views::transform( [ &swapchain ]( uint32_t index ) { return swapchain.getView( index ); } );

    auto depth_view = ranges::views::single( depth_image_view );

    for ( auto view : image_views )
    {
        auto framebuffer_builder = vkwrap::FramebufferBuilder{};
        framebuffer_builder.withAttachments( ranges::views::concat( ranges::views::single( view ), depth_view ) )
            .withWidth( extent.width )
            .withHeight( extent.height )
            .withRenderPass( render_pass )
            .withLayers( 1 );

        framebuffers.push_back( framebuffer_builder.make( logical_device ) );
    }

    return framebuffers;
}

static constexpr auto k_max_frames_in_flight = uint32_t{ 2 };

vkwrap::SwapchainReqs
getSwapchainRequirements( vk::SurfaceKHR surface )
{
    auto formats = std::to_array<vkwrap::SwapchainReqsBuilder::WeightFormat>(
        { { .property = { vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear }, .weight = 0 },
          { .property = { vk::Format::eR8G8B8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear }, .weight = 0 } } );

    auto modes = std::to_array<vkwrap::SwapchainReqsBuilder::WeightMode>(
        { { .property = vk::PresentModeKHR::eFifo, .weight = 1000 }, //
          { .property = vk::PresentModeKHR::eMailbox, .weight = 0 } } );

    auto requirement_builder = vkwrap::SwapchainReqsBuilder{};
    requirement_builder.withMinImageCount( k_max_frames_in_flight )
        .withFormats( formats )
        .withModes( modes )
        .withSurface( surface );

    return requirement_builder.make();
}

vkwrap::Swapchain
createSwapchain(
    vk::PhysicalDevice physical_device,
    vk::Device logical_device,
    vk::SurfaceKHR surface,
    vkwrap::Queue graphics_queue,
    vkwrap::Queue present_queue,
    const vkwrap::SwapchainReqs& swapchain_requirements )
{
    auto min_image_count = std::max(
        swapchain_requirements.getMinImageCount(),
        physical_device.getSurfaceCapabilitiesKHR( surface ).minImageCount );

    auto swapchain_builder = vkwrap::SwapchainBuilder{};
    swapchain_builder.withQueues( std::array{ graphics_queue, present_queue } )
        .withImageExtent( vkwrap::getSurfaceExtent( physical_device, surface ) )
        .withSurface( surface )
        .withMinImageCount( min_image_count )
        .withImageUsage( vk::ImageUsageFlagBits::eColorAttachment )
        .withPreTransform( vk::SurfaceTransformFlagBitsKHR::eIdentity )
        .withCompositeAlpha( vk::CompositeAlphaFlagBitsKHR::eOpaque )
        .withClipped( false )
        .withOldSwapchain( {} );

    auto new_swapchain = swapchain_builder.make( logical_device, physical_device, swapchain_requirements );

    return new_swapchain;
}

struct FrameSyncPrimitives
{
    vk::UniqueSemaphore image_availible_semaphore;
    vk::UniqueSemaphore render_finished_semaphore;
    vk::UniqueFence in_flight_fence;
};

struct FrameRenderingInfos
{
    std::vector<FrameSyncPrimitives> sync_primitives;
    std::vector<vk::UniqueCommandBuffer> imgui_command_buffers;
};

FrameRenderingInfos
createRenderInfos( vk::Device logical_device, vkwrap::CommandPool& command_pool )
{
    FrameRenderingInfos frame_render_info;

    auto frames = k_max_frames_in_flight;

    for ( [[maybe_unused]] auto&& i = uint32_t{ 0 }; i < frames; ++i )
    {
        auto primitives = FrameSyncPrimitives{
            .image_availible_semaphore = logical_device.createSemaphoreUnique( {} ),
            .render_finished_semaphore = logical_device.createSemaphoreUnique( {} ),
            .in_flight_fence = logical_device.createFenceUnique( { .flags = vk::FenceCreateFlagBits::eSignaled } ) };

        frame_render_info.sync_primitives.push_back( std::move( primitives ) );
    }

    frame_render_info.imgui_command_buffers = command_pool.createCmdBuffers( static_cast<uint32_t>( frames ) );

    return frame_render_info;
}

vkwrap::Image
createDepthBuffer( const vkwrap::Swapchain& swapchain, ranges::range auto&& queues, vkwrap::Mman& manager )
{
    auto [ width, height ] = swapchain.getExtent();
    auto builder = vkwrap::ImageBuilder{};

    auto depth_image = builder.withExtent( { .width = width, .height = height, .depth = 1 } )
                           .withFormat( vk::Format::eD32Sfloat )
                           .withTiling( vk::ImageTiling::eOptimal )
                           .withImageType( vk::ImageType::e2D )
                           .withQueues( queues )
                           .withSampleCount( vk::SampleCountFlagBits::e1 )
                           .withArrayLayers( 1 )
                           .withUsage( vk::ImageUsageFlagBits::eDepthStencilAttachment )
                           .make( manager );

    return depth_image;
}

bool
shouldRecreateSwapchain( vk::Result result )
{
    return ( result == vk::Result::eErrorOutOfDateKHR || result == vk::Result::eSuboptimalKHR );
}

class KtxError : public std::runtime_error
{
  public:
    KtxError( ktx_error_code_e ec )
        : std::runtime_error{ ktxErrorString( ec ) },
          m_ec{ ec }
    {
    }

    auto getErrorCode() { return m_ec; }

  private:
    ktx_error_code_e m_ec;
};

void
checkKtxResult( ktx_error_code_e ec )
{
    if ( ec != KTX_SUCCESS )
    {
        throw KtxError{ ec };
    }
}

class UniqueKtxTexture
{
    static ktxTexture* createHandle( const std::filesystem::path& filepath, ktxTextureCreateFlags flags )
    {
        ktxTexture* handle = nullptr;
        auto res = ktxTexture_CreateFromNamedFile( filepath.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &handle );
        checkKtxResult( res );
        return handle;
    }

  public:
    UniqueKtxTexture( const std::filesystem::path& filepath, ktxTextureCreateFlags flags )
        : m_handle{ createHandle( filepath, flags ) }
    {
    }

    ktxTexture* get() const { return m_handle.get(); }
    ktxTexture* operator->() { return get(); }

    ktx_uint8_t* getData() { return ktxTexture_GetData( m_handle.get() ); }
    ktx_size_t getSize() { return ktxTexture_GetDataSize( m_handle.get() ); }

    ktx_size_t getImageOffset( ktx_size_t level, ktx_size_t layer, ktx_size_t /* maybe */ slice )
    {
        ktx_size_t offset = 0;
        auto res = ktxTexture_GetImageOffset( m_handle.get(), level, layer, slice, &offset );
        checkKtxResult( res );
        return offset;
    }

  private:
    static constexpr auto k_deleter = []( ktxTexture* ptr ) {
        if ( ptr )
        {
            ktxTexture_Destroy( ptr );
        }
    };

    std::unique_ptr<ktxTexture, decltype( k_deleter )> m_handle = nullptr;
};

vkwrap::Image
createTextureImage(
    ranges::range auto&& queues,
    vkwrap::Mman& manager,
    const std::filesystem::path& filepath = "texture.ktx" )
{
    auto ktx_texture = UniqueKtxTexture{ filepath, KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT };

    auto image_builder = vkwrap::ImageBuilder{};
    auto texture_image =
        image_builder.withExtent( { .width = ktx_texture->baseWidth, .height = ktx_texture->baseHeight, .depth = 1 } )
            .withFormat( vk::Format::eR8G8B8A8Srgb )
            .withArrayLayers( ktx_texture->numLayers )
            .withTiling( vk::ImageTiling::eOptimal )
            .withSampleCount( vk::SampleCountFlagBits::e1 )
            .withUsage( vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled )
            .withImageType( vk::ImageType::e2D )
            .withQueues( queues )
            .make( manager );

    auto buffer_builder = vkwrap::BufferBuilder{};
    auto ktx_texture_size = ktx_texture.getSize();

    auto staging_buffer = buffer_builder.withSize( ktx_texture_size )
                              .withQueues( queues )
                              .withUsage( vk::BufferUsageFlagBits::eTransferSrc )
                              .make( manager );

    staging_buffer.update( *ktx_texture.getData(), ktx_texture_size );
    texture_image.transit( vk::ImageLayout::eTransferDstOptimal );

    auto layer_offset_counter = [ &ktx_texture ]( uint32_t layer ) {
        ktx_size_t offset = ktx_texture.getImageOffset( 0, layer, 0 );

        return vkwrap::Mman::Region{
            .buffer_offset = offset,
            .buffer_row_length = 0,
            .buffer_image_height = 0,
            .aspect_mask = vk::ImageAspectFlagBits::eColor,
            .image_offset = vk::Offset3D{ 0, 0, 0 },
        };
    };

    texture_image.update( staging_buffer.get(), layer_offset_counter );
    texture_image.transit( vk::ImageLayout::eShaderReadOnlyOptimal );

    return texture_image;
}

template <ranges::contiguous_range Memory>
vkwrap::Buffer
createDeviceLocalBuffer(
    ranges::range auto&& queues,
    Memory&& memory,
    vkwrap::Mman& manager,
    vk::BufferUsageFlags usage_flags )
{
    auto buffer_builder = vkwrap::BufferBuilder{};
    buffer_builder.withQueues( queues );

    auto size = memory.size() * sizeof( ranges::range_value_t<Memory> );
    auto buffer = buffer_builder.withSize( size ).withUsage( usage_flags ).make( manager );

    auto staging_buffer =
        buffer_builder.withSize( size ).withUsage( vk::BufferUsageFlagBits::eTransferSrc ).make( manager );

    staging_buffer.update( *memory.data(), size );
    buffer.update( staging_buffer.get() );

    return buffer;
}

vkwrap::Buffer
createVertexBuffer( ranges::range auto&& queues, const chunk::ChunkMesher& mesher, vkwrap::Mman& manager )
{
    auto memory = std::span{ mesher.getVerticesData(), mesher.getVerticesCount() };
    return createDeviceLocalBuffer(
        queues,
        memory,
        manager,
        vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst );
}

vkwrap::Buffer
createIndexBuffer( ranges::range auto&& queues, const chunk::ChunkMesher& mesher, vkwrap::Mman& manager )
{
    auto memory = std::span{ mesher.getIndicesData(), mesher.getIndicesCount() };
    return createDeviceLocalBuffer(
        queues,
        memory,
        manager,
        vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst );
}

void
runApplication( std::span<const char*> command_line_args )
{
    AppOptions options;

    if ( auto parsed = parseOptions( command_line_args ); !parsed )
    {
        return;
    } else
    {
        options = parsed.value();
    }

    spdlog::cfg::load_env_levels();
    // Use `export SPDLOG_LEVEL=debug` to set maximum logging level
    // Or `export SPDLOG_LEVEL=warn` to print only warnings and errors
    vkwrap::initializeLoader(); // Load basic functions that are instance independent

    auto glfw_instance = glfw::Instance{};
    auto window = glfw::wnd::Window{ glfw::wnd::WindowConfig{ .title = "MinCraft" } };

    auto vk_instance = createInstance( glfw_instance, options.validation ).instance;
    auto surface = window.createSurface( vk_instance );
    auto swapchain_requirements = getSwapchainRequirements( surface.get() );
    auto physical_device = pickPhysicalDevice( vk_instance, surface.get(), swapchain_requirements );

    auto [ logical_device, graphics_queue, present_queue ] =
        createLogicalDeviceQueues( physical_device.get(), surface.get() );

    auto queues = std::array{ graphics_queue, present_queue };

    auto command_pool =
        vkwrap::CommandPool{ logical_device, graphics_queue, vk::CommandPoolCreateFlagBits::eResetCommandBuffer };

    auto one_time_cmd = vkwrap::OneTimeCommand{ command_pool, graphics_queue.get() };

    auto swapchain = createSwapchain(
        physical_device.get(),
        logical_device.get(),
        surface.get(),
        graphics_queue,
        present_queue,
        swapchain_requirements );

    chunk::ChunkMesher mesher{};
    mesher.meshRenderArea();

    vkwrap::Mman manager{
        vkwrap::VulkanVersion::e_version_1_3,
        vk_instance,
        physical_device.get(),
        logical_device,
        graphics_queue.get(),
        command_pool.get() };

    auto depth_image = createDepthBuffer( swapchain, queues, manager );

    // create descriptor set layout
    vk::DescriptorSetLayoutBinding ubo_layout_binding{
        .binding = 0,
        .descriptorType = vk::DescriptorType::eUniformBuffer,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eVertex,
    };

    vk::DescriptorSetLayoutBinding sampler_layout_binding{
        .binding = 1,
        .descriptorType = vk::DescriptorType::eCombinedImageSampler,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eFragment,
        .pImmutableSamplers = nullptr };

    std::array<vk::DescriptorSetLayoutBinding, 2> bindings = { ubo_layout_binding, sampler_layout_binding };
    vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_info{
        .bindingCount = static_cast<uint32_t>( bindings.size() ),
        .pBindings = bindings.data() };

    vk::UniqueDescriptorSetLayout descriptor_set_layout =
        logical_device->createDescriptorSetLayoutUnique( descriptor_set_layout_info );

    vkwrap::ShaderModule vert_shader_module{ "vertex_shader.spv", logical_device.get() };
    vkwrap::ShaderModule frag_shader_module{ "fragment_shader.spv", logical_device.get() };

    auto vertex_info = mesher.getVertexInfo();

    vk::SubpassDependency dependency{
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,

        .srcStageMask =
            vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests,
        .dstStageMask =
            vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests,
        .srcAccessMask = {},
        .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite,
    };

    auto pipeline_builder = vkwrap::DefaultPipelineBuilder{};
    auto pipeline = pipeline_builder.withVertexShader( vert_shader_module )
                        .withFragmentShader( frag_shader_module )
                        .withPipelineLayout( logical_device.get(), std::array{ descriptor_set_layout.get() } )
                        .withAttributeDescriptions( vertex_info.attribute_descr )
                        .withBindingDescriptions( vertex_info.binding_descr )
                        .withColorAttachment( swapchain.getFormat() )
                        .withDepthAttachment( vk::Format::eD32Sfloat )
                        .withSubpassDependencies( std::array{ dependency } )
                        .withRenderPass( logical_device.get() )
                        .createPipeline( logical_device.get() );

    auto render_pass = pipeline_builder.getRenderPass();
    auto pipeline_layout = pipeline_builder.getPipelineLayout();

    auto framebuffers = createFramebuffers( swapchain, depth_image.getView(), logical_device, render_pass );
    auto render_infos = createRenderInfos( logical_device, command_pool );

    auto imgui_resources = imgw::ImGuiResources{ imgw::ImGuiResources::ImGuiResourcesInitInfo{
        .instance = vk_instance,
        .window = window.get(),
        .physical_device = physical_device.get(),
        .logical_device = logical_device,
        .graphics = graphics_queue,
        .swapchain = swapchain,
        .upload_context = one_time_cmd,
        .render_pass = render_pass } };

    // create sampler
    vkwrap::SamplerBuilder sampler_builder{};
    auto sampler = sampler_builder.withMagFilter( vk::Filter::eNearest )
                       .withMinFilter( vk::Filter::eLinear )
                       .withAddressModeU( vk::SamplerAddressMode::eRepeat )
                       .withAddressModeV( vk::SamplerAddressMode::eRepeat )
                       .withAddressModeW( vk::SamplerAddressMode::eRepeat )
                       .withAnisotropyEnable( VK_FALSE )
                       .withBorderColor( vk::BorderColor::eFloatOpaqueBlack )
                       .withUnnormalizedCoordinates( VK_FALSE )
                       .withCompareOp( vk::CompareOp::eAlways )
                       .make( logical_device, physical_device.get() );

    auto texture_image = createTextureImage( queues, manager );
    auto buffer_builder = vkwrap::BufferBuilder{};

    auto vertex_buffer = createVertexBuffer( queues, mesher, manager );
    auto index_buffer = createIndexBuffer( queues, mesher, manager );

    std::array<vkwrap::Buffer, k_max_frames_in_flight> uniform_buffers{
        buffer_builder.withSize( sizeof( UniformBufferObject ) )
            .withQueues( std::array{ graphics_queue } )
            .withUsage( vk::BufferUsageFlagBits::eUniformBuffer )
            .make( manager ),
        buffer_builder.withSize( sizeof( UniformBufferObject ) )
            .withQueues( std::array{ graphics_queue } )
            .withUsage( vk::BufferUsageFlagBits::eUniformBuffer )
            .make( manager ),

    };

    auto pool_sizes = std::array{
        vk::DescriptorPoolSize{
            .type = vk::DescriptorType::eUniformBuffer,
            .descriptorCount = static_cast<uint32_t>( k_max_frames_in_flight ) },
        vk::DescriptorPoolSize{
            .type = vk::DescriptorType::eCombinedImageSampler,
            .descriptorCount = static_cast<uint32_t>( k_max_frames_in_flight ) } };

    vk::DescriptorPoolCreateInfo pool_info{
        .maxSets = static_cast<uint32_t>( k_max_frames_in_flight ),
        .poolSizeCount = static_cast<uint32_t>( pool_sizes.size() ),
        .pPoolSizes = pool_sizes.data(),
    };

    auto descriptor_pool = std::move( logical_device->createDescriptorPoolUnique( pool_info ) );

    std::vector<vk::DescriptorSetLayout> layouts( k_max_frames_in_flight, descriptor_set_layout.get() );

    vk::DescriptorSetAllocateInfo alloc_info{
        .descriptorPool = descriptor_pool.get(),
        .descriptorSetCount = static_cast<uint32_t>( k_max_frames_in_flight ),
        .pSetLayouts = layouts.data() };

    auto descriptor_sets = logical_device->allocateDescriptorSets( alloc_info );

    for ( size_t i = 0; i < k_max_frames_in_flight; i++ )
    {
        vk::DescriptorBufferInfo buffer_info{
            .buffer = uniform_buffers[ i ].get(),
            .offset = 0,
            .range = sizeof( UniformBufferObject ) };

        vk::DescriptorImageInfo image_info{
            .sampler = sampler.get(),
            .imageView = texture_image.getView(),
            .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal };

        auto write_descriptor_sets = std::array{
            vk::WriteDescriptorSet{
                .dstSet = descriptor_sets[ i ],
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eUniformBuffer,
                .pBufferInfo = &buffer_info },
            vk::WriteDescriptorSet{
                .dstSet = descriptor_sets[ i ],
                .dstBinding = 1,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                .pImageInfo = &image_info } };

        logical_device->updateDescriptorSets(
            static_cast<uint32_t>( write_descriptor_sets.size() ),
            write_descriptor_sets.data(),
            0,
            nullptr );
    }

    auto recreate_swapchain_wrapped = [ &, &logical_device = logical_device ]() {
        swapchain.recreate();
        depth_image = createDepthBuffer( swapchain, queues, manager );
        framebuffers = createFramebuffers( swapchain, depth_image.getView(), logical_device, render_pass );
    };

    uint32_t current_frame = 0;

    auto fill_command_buffer = [ & ]( vk::CommandBuffer& cmd, uint32_t image_index, vk::Extent2D extent ) {
        const auto clear_values = std::array{
            vk::ClearValue{ .color = { utils::hexToRGBA( 0x181818ff ) } },
            vk::ClearValue{ .depthStencil = { .depth = 1.0f, .stencil = 0 } } };

        const auto render_pass_info = vk::RenderPassBeginInfo{
            .renderPass = render_pass,
            .framebuffer = framebuffers.at( image_index ).get(),
            .renderArea = { vk::Offset2D{ 0, 0 }, extent },
            .clearValueCount = static_cast<uint32_t>( clear_values.size() ),
            .pClearValues = clear_values.data() };

        cmd.reset();
        cmd.begin( vk::CommandBufferBeginInfo{ .flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse } );
        cmd.beginRenderPass( render_pass_info, vk::SubpassContents::eInline );

        cmd.bindPipeline( vk::PipelineBindPoint::eGraphics, pipeline );
        cmd.bindVertexBuffers( 0, vertex_buffer.get(), vk::DeviceSize{ 0 } );
        cmd.bindIndexBuffer( index_buffer.get(), 0, chunk::ChunkMesher::k_index_type );

        const auto [ width, height ] = extent;
        vk::Viewport viewport{
            .x = 0.0f,
            .y = 0.0f,
            .width = static_cast<float>( width ),
            .height = static_cast<float>( height ),
            .minDepth = 0.0f,
            .maxDepth = 1.0f };

        const auto scissor = vk::Rect2D{ .offset = { 0, 0 }, .extent = extent };

        cmd.setViewport( 0, viewport );
        cmd.setScissor( 0, scissor );

        cmd.bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            pipeline_layout,
            0,
            1,
            &descriptor_sets.at( current_frame ),
            0,
            nullptr );

        cmd.drawIndexed( static_cast<uint32_t>( mesher.getIndicesCount() ), 1, 0, 0, 0 );

        imgui_resources.fillCommandBuffer( cmd );

        cmd.endRenderPass();
        cmd.end();
    };

    auto render_frame =
        [ &, &logical_device = logical_device, &graphics_queue = graphics_queue, &present_queue = present_queue ]() {
            auto& current_frame_data = render_infos.sync_primitives.at( current_frame );
            auto& command_buffer = render_infos.imgui_command_buffers.at( current_frame );
            logical_device->waitForFences( current_frame_data.in_flight_fence.get(), VK_TRUE, UINT64_MAX );

            // update framebuffer
            UniformBufferObject ubo{};

            static float yaw = 0;
            static float pitch = 0;

            pitch -= 0.5;

            if ( pitch > 89.0f )
            {
                pitch = 89.0f;
            }

            if ( pitch < -89.0f )
            {
                pitch = -89.0f;
            }

            glm::vec3 front;
            front.y = cos( glm::radians( yaw ) ) * cos( glm::radians( pitch ) );
            front.z = sin( glm::radians( pitch ) );
            front.x = sin( glm::radians( yaw ) ) * cos( glm::radians( pitch ) );
            front = glm::normalize( front );

            ubo.model = glm::mat4( 1.0f );
            ubo.view = glm::lookAt(
                glm::vec3{ 0.0f, 0.0f, 10.0f },
                glm::vec3{ 0.0f, 0.0f, 10.0f } + front,
                glm::vec3{ 0.0f, 0.0f, 1.0f } );

            ubo.proj = glm::perspective(
                glm::radians( 45.0f ),
                swapchain.getExtent().width / (float)swapchain.getExtent().height,
                0.1f,
                1000.0f );

            ubo.proj[ 1 ][ 1 ] *= -1;

            ubo.origin_pos = glm::vec2{ mesher.getRenderAreaRight().x, mesher.getRenderAreaRight().y };

            uniform_buffers[ current_frame ].update( ubo, sizeof( ubo ) );

            auto [ acquire_result, image_index ] =
                swapchain.acquireNextImage( current_frame_data.image_availible_semaphore.get() );

            if ( shouldRecreateSwapchain( acquire_result ) )
            {
                recreate_swapchain_wrapped();
            }

            fill_command_buffer( command_buffer.get(), image_index, swapchain.getExtent() );

            const auto cmds = std::array{ *command_buffer };

            vk::PipelineStageFlags wait_stages = vk::PipelineStageFlagBits::eColorAttachmentOutput;

            const auto submit_info = vk::SubmitInfo{
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = std::addressof( *current_frame_data.image_availible_semaphore ),
                .pWaitDstStageMask = &wait_stages,
                .commandBufferCount = cmds.size(),
                .pCommandBuffers = cmds.data(),
                .signalSemaphoreCount = 1,
                .pSignalSemaphores = std::addressof( *current_frame_data.render_finished_semaphore ) };

            logical_device->resetFences( *current_frame_data.in_flight_fence );
            graphics_queue.submit( submit_info, *current_frame_data.in_flight_fence );

            vk::PresentInfoKHR present_info = {
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = std::addressof( *current_frame_data.render_finished_semaphore ),
                .swapchainCount = 1,
                .pSwapchains = &swapchain.get(),
                .pImageIndices = &image_index };

            vk::Result result_present = present_queue.presentKHRWithOutOfDate( present_info );
            if ( shouldRecreateSwapchain( result_present ) )
            {
                recreate_swapchain_wrapped();
            }

            current_frame = ( current_frame + 1 ) % k_max_frames_in_flight;
        };

    auto draw_gui = []() {
        ImGui::ShowDemoWindow();
    };

    auto loop = [ & ]() {
        imgui_resources.newFrame();
        draw_gui();
        imgui_resources.renderFrame();
        render_frame();
    };

    auto renderer_thread = std::jthread{ [ &, &logical_device = logical_device ]( std::stop_token stop ) {
        while ( !stop.stop_requested() )
        {
            loop();
        }
        logical_device->waitIdle(); // To shut down nicely
    } };

    while ( window.running() )
    {
        glfwPollEvents();
    }

    renderer_thread.request_stop();
}

} // namespace

int
main( int argc, const char** argv )
try
{
    runApplication( std::span{ argv, static_cast<size_t>( argc ) } );
} catch ( vkwrap::UnsupportedError& e )
{
    fmt::print( "Unsupported error: {}\n", e.what() );
    for ( unsigned i = 0; auto&& entry : e )
    {
        fmt::print( "[{}]. {}: {}\n", i++, vkwrap::unsupportedTagToStr( entry.tag ), entry.name );
    }
} catch ( vk::Error& e )
{
    fmt::print( "Vulkan error: {}\n", e.what() );
} catch ( std::exception& e )
{
    fmt::print( "Error: {}\n", e.what() );
}
