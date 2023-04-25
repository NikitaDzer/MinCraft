#include "common/vulkan_include.h"
#include "utils/color.h"

#include "vkwrap/command.h"
#include "vkwrap/device.h"
#include "vkwrap/framebuffer.h"
#include "vkwrap/instance.h"
#include "vkwrap/pipeline.h"
#include "vkwrap/queues.h"

#include "vkwrap/swapchain.h"

#include "glfw/window.h"
#include "gui/gui.hpp"

#include <spdlog/cfg/env.h>
#include <spdlog/fmt/bundled/core.h>
#include <spdlog/fmt/bundled/format.h>

#include <popl/popl.hpp>

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

namespace
{

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
pickPhysicalDevice( vk::Instance instance, vk::SurfaceKHR surface )
{
    vkwrap::PhysicalDeviceSelector physical_selector;

    auto depth_weight = []( vkwrap::PhysicalDeviceInfo info ) -> int {
        auto candidates = std::to_array<vk::Format>(
            { vk::Format::eD32Sfloat, vk::Format::eD32SfloatS8Uint, vk::Format::eD24UnormS8Uint } );

        auto is_compatible =
            ranges::any_of( candidates, [ physical_device = info.device ]( vk::Format format ) -> bool {
                auto props = physical_device.getFormatProperties( format );
                return ( props.optimalTilingFeatures & vk::FormatFeatureFlagBits::eDepthStencilAttachment ) ==
                    vk::FormatFeatureFlagBits::eDepthStencilAttachment;
            } );

        return ( is_compatible ? 0 : -1 );
    };

    // Calculate weights depending on the requirements. Should be expanded to check for Format capabilities e.t.c. Help
    // needed :)
    auto weight_functor = [ depth_weight ]( vkwrap::PhysicalDeviceInfo info ) {
        return depth_weight( info );
    };

    physical_selector.withExtensions( std::array{ VK_KHR_SWAPCHAIN_EXTENSION_NAME } )
        .withTypes( std::array{ vk::PhysicalDeviceType::eDiscreteGpu, vk::PhysicalDeviceType::eIntegratedGpu } )
        .withPresent( surface )
        .withVersion( vkwrap::VulkanVersion::e_version_1_3 )
        .withWeight( weight_functor );

    auto suitable_devices = physical_selector.make( instance );

    if ( suitable_devices.empty() )
    {
        throw std::runtime_error{ "No suitable physical devices found" };
    }

    return suitable_devices.at( 0 ).device;
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
recreateFramebuffers( vkwrap::Swapchain& swapchain, vk::Device logical_device, vk::RenderPass render_pass )
{
    auto extent = swapchain.getExtent();
    Framebuffers framebuffers;

    auto image_views = ranges::views::iota( uint32_t{ 0 }, swapchain.getImagesCount() ) |
        ranges::views::transform( [ &swapchain ]( uint32_t index ) { return swapchain.getView( index ); } );

    for ( auto view : image_views )
    {
        auto framebuffer_builder = vkwrap::FramebufferBuilder{};
        framebuffer_builder.withAttachments( ranges::views::single( view ) )
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
getSwapchainRequirements()
{
    auto formats = std::to_array<vkwrap::SwapchainReqsBuilder::WeightFormat>(
        { { .format = { vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear }, .weight = 0 },
          { .format = { vk::Format::eR8G8B8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear }, .weight = 0 } } );

    auto modes = std::to_array<vkwrap::SwapchainReqsBuilder::WeightMode>(
        { { .mode = vk::PresentModeKHR::eFifo, .weight = 1000 }, //
          { .mode = vk::PresentModeKHR::eMailbox, .weight = 0 } } );

    auto requirement_builder = vkwrap::SwapchainReqsBuilder{};
    requirement_builder.withMinImageCount( k_max_frames_in_flight ).withFormats( formats ).withModes( modes );

    return requirement_builder.make();
}

vkwrap::Swapchain
createSwapchain(
    vk::PhysicalDevice physical_device,
    vk::Device logical_device,
    vk::SurfaceKHR surface,
    vkwrap::Queue graphics_queue,
    vkwrap::Queue present_queue,
    vkwrap::SwapchainReqs swapchain_requirements )
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
    auto physical_device = pickPhysicalDevice( vk_instance, surface.get() );

    auto [ logical_device, graphics_queue, present_queue ] =
        createLogicalDeviceQueues( physical_device.get(), surface.get() );

    auto command_pool =
        vkwrap::CommandPool{ logical_device, graphics_queue, vk::CommandPoolCreateFlagBits::eResetCommandBuffer };

    auto one_time_cmd = vkwrap::OneTimeCommand{ command_pool, graphics_queue.get() };

    auto swapchain = createSwapchain(
        physical_device.get(),
        logical_device,
        surface.get(),
        graphics_queue,
        present_queue,
        getSwapchainRequirements() );

    auto pipeline_builder = vkwrap::DefaultPipelineBuilder{};
    auto render_pass =
        pipeline_builder.withColorAttachment( swapchain.getFormat() ).withRenderPass( logical_device ).getRenderPass();

    auto imgui_resources = imgw::ImGuiResources{ imgw::ImGuiResources::ImGuiResourcesInitInfo{
        .instance = vk_instance,
        .window = window.get(),
        .physical_device = physical_device.get(),
        .logical_device = logical_device,
        .graphics = graphics_queue,
        .swapchain = swapchain,
        .upload_context = one_time_cmd,
        .render_pass = render_pass } };

    auto framebuffers = recreateFramebuffers( swapchain, logical_device, render_pass );
    auto render_infos = createRenderInfos( logical_device, command_pool );

    auto recreate_swapchain_wrapped = [ &, &logical_device = logical_device ]() {
        swapchain.recreate();
        framebuffers = recreateFramebuffers( swapchain, logical_device, render_pass );
    };

    uint32_t current_frame = 0;

    auto fill_command_buffer = [ &imgui_resources,
                                 &render_pass,
                                 &framebuffers ]( vk::CommandBuffer& cmd, uint32_t image_index, vk::Extent2D extent ) {
        static constexpr auto k_clear_color = vk::ClearValue{ .color = { utils::hexToRGBA( 0x181818ff ) } };

        const auto render_pass_info = vk::RenderPassBeginInfo{
            .renderPass = render_pass,
            .framebuffer = framebuffers[ image_index ].get(),
            .renderArea = { vk::Offset2D{ 0, 0 }, extent },
            .clearValueCount = 1,
            .pClearValues = &k_clear_color };

        cmd.reset();
        cmd.begin( vk::CommandBufferBeginInfo{ .flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse } );
        cmd.beginRenderPass( render_pass_info, vk::SubpassContents::eInline );

        imgui_resources.fillCommandBuffer( cmd );

        cmd.endRenderPass();
        cmd.end();
    };

    auto render_frame =
        [ &, &logical_device = logical_device, &graphics_queue = graphics_queue, &present_queue = present_queue ]() {
            auto& current_frame_data = render_infos.sync_primitives.at( current_frame );
            auto& command_buffer = render_infos.imgui_command_buffers.at( current_frame );
            logical_device->waitForFences( current_frame_data.in_flight_fence.get(), VK_TRUE, UINT64_MAX );

            const auto acquire_info = vk::AcquireNextImageInfoKHR{
                .swapchain = swapchain.get(),
                .timeout = UINT64_MAX,
                .semaphore = current_frame_data.image_availible_semaphore.get(),
                .fence = nullptr,
                .deviceMask = 1 };

            uint32_t image_index;
            try
            {
                image_index = logical_device->acquireNextImage2KHR( acquire_info ).value;
            } catch ( vk::OutOfDateKHRError& )
            {
                recreate_swapchain_wrapped();
                return;
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
            if ( result_present == vk::Result::eSuboptimalKHR || result_present == vk::Result::eErrorOutOfDateKHR )
            {
                recreate_swapchain_wrapped();
            }

            current_frame = ( current_frame + 1 ) % k_max_frames_in_flight;
        };

    auto terminate = [ &logical_device = logical_device ]() {
        logical_device->waitIdle();
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

    while ( window.running() )
    {
        glfwPollEvents();
        loop();
    }

    terminate();
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
