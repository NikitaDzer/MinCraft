#include "common/vulkan_include.h"
#include "utils/color.h"

#include "vkwrap/buffer.h"
#include "vkwrap/command.h"
#include "vkwrap/descriptors.h"
#include "vkwrap/device.h"
#include "vkwrap/framebuffer.h"
#include "vkwrap/image.h"
#include "vkwrap/image_view.h"
#include "vkwrap/instance.h"
#include "vkwrap/pipeline.h"
#include "vkwrap/queues.h"
#include "vkwrap/render_pass.h"
#include "vkwrap/sampler.h"
#include "vkwrap/swapchain.h"

#include "chunk/chunk_man.h"
#include "chunk/chunk_mesher.h"

#include "glfw/input/keyboard.h"
#include "glfw/input/mouse.h"
#include "glfw/window.h"

#include "gui/gui.h"

#include "camera.h"
#include "glm_include.h"
#include "info_gui.h"

#include <ktx.h>
#include <ktxvulkan.h>

#include <spdlog/cfg/env.h>
#include <spdlog/fmt/bundled/core.h>
#include <spdlog/fmt/bundled/format.h>

#include <boost/program_options.hpp>

#include <range/v3/algorithm/any_of.hpp>
#include <range/v3/algorithm/find_if.hpp>
#include <range/v3/view/iota.hpp>
#include <range/v3/view/single.hpp>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <functional>
#include <future>
#include <iterator>
#include <numeric>
#include <sstream>
#include <string>
#include <utility>

namespace
{

struct UniformBufferObject
{
    glm::mat4 model = {};
    glm::mat4 view = {};
    glm::mat4 proj = {};
    glm::vec2 origin_pos = {};
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
    bool uncapped_fps = false;
};

namespace po = boost::program_options;

std::optional<AppOptions>
parseOptions( std::span<const char*> command_line_args )
{
    po::options_description desc( "Available options" );
    desc.add_options()( "help,h", "Print this help message" )( "debug,d", "Use validation layers" )(
        "f,uncap",
        "Uncapped fps always" );

    po::variables_map vm;
    po::store( po::parse_command_line( command_line_args.size(), command_line_args.data(), desc ), vm );
    po::notify( vm );

    if ( vm.count( "help" ) )
    {
        std::stringstream ss;
        ss << desc;
        fmt::print( "{}", ss.str() );
        return std::nullopt;
    }

#ifndef NDEBUG
    const bool validation = true; // Enable validation layers in debug build
#else
    const bool validation = vm.count( "debug" );
#endif

    const bool uncapped = vm.count( "uncap" );

    return AppOptions{ .validation = validation, .uncapped_fps = uncapped };
}

vkwrap::PhysicalDevice
pickPhysicalDevice( vk::Instance instance, vkwrap::SwapchainReqs swapchain_requirements )
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

    auto supported_features = physical_device.getFeatures();

    device_builder.withExtensions( std::array{ VK_KHR_SWAPCHAIN_EXTENSION_NAME } )
        .withGraphicsQueue( graphics )
        .withPresentQueue( surface, present )
        .withFeatures( supported_features );

    auto logical_device = device_builder.make( physical_device );
    VULKAN_HPP_DEFAULT_DISPATCHER.init( logical_device );

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
getSwapchainRequirements( vk::SurfaceKHR surface, bool uncapped_always = false )
{
    auto formats = std::to_array<vkwrap::SwapchainReqsBuilder::WeightFormat>(
        { { .property = { vk::Format::eB8G8R8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear }, .weight = 0 },
          { .property = { vk::Format::eR8G8B8A8Unorm, vk::ColorSpaceKHR::eSrgbNonlinear }, .weight = 0 } } );

    const auto weight_immediate = ( uncapped_always ? 0 : 100 );
    auto modes = std::to_array<vkwrap::SwapchainReqsBuilder::WeightMode>(
        { { .property = vk::PresentModeKHR::eFifo, .weight = 50 }, //
          { .property = vk::PresentModeKHR::eMailbox, .weight = weight_immediate },
          { .property = vk::PresentModeKHR::eImmediate, .weight = weight_immediate } } );

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
    std::vector<vkwrap::Buffer> uniform_buffers;
};

FrameRenderingInfos
createRenderInfos(
    vk::Device logical_device,
    vkwrap::CommandPool& command_pool,
    ranges::range auto&& queues,
    vkwrap::Mman& manager )
{
    FrameRenderingInfos frame_render_info;

    auto frames = k_max_frames_in_flight;
    auto buffer_builder = vkwrap::BufferBuilder{};
    buffer_builder.withQueues( queues );

    for ( [[maybe_unused]] auto i = uint32_t{ 0 }; i < frames; ++i )
    {
        auto primitives = FrameSyncPrimitives{
            .image_availible_semaphore = logical_device.createSemaphoreUnique( {} ),
            .render_finished_semaphore = logical_device.createSemaphoreUnique( {} ),
            .in_flight_fence = logical_device.createFenceUnique( { .flags = vk::FenceCreateFlagBits::eSignaled } ) };

        frame_render_info.sync_primitives.push_back( std::move( primitives ) );

        auto buffer = buffer_builder.withSize( sizeof( UniformBufferObject ) )
                          .withUsage( vk::BufferUsageFlagBits::eUniformBuffer )
                          .make( manager );

        frame_render_info.uniform_buffers.push_back( std::move( buffer ) );
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
    static ktxTexture* createHandle( const std::filesystem::path& filepath )
    {
        ktxTexture* handle = nullptr;
        auto res = ktxTexture_CreateFromNamedFile( filepath.c_str(), KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &handle );
        checkKtxResult( res );
        return handle;
    }

  public:
    UniqueKtxTexture( const std::filesystem::path& filepath )
        : m_handle{ createHandle( filepath ) }
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
createTextureImage( ranges::range auto&& queues, vkwrap::Mman& manager, const std::filesystem::path& filepath )
{
    auto ktx_texture = UniqueKtxTexture{ filepath };

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
    buffer.update( *memory.data(), size );

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

class KeyboardStateTracker
{
  public:
    KeyboardStateTracker( GLFWwindow* window )
        : m_handler_ptr{ &glfw::input::KeyboardHandler::instance( window ) }
    {
        assert( window );
    }

    KeyboardStateTracker( glfw::input::KeyboardHandler& handler )
        : m_handler_ptr{ &handler }
    {
    }

  public:
    template <ranges::range Range>
        requires std::same_as<ranges::range_value_t<Range>, glfw::input::KeyIndex>
    void monitor( Range&& keys )
    {
        m_state_map.clear();
        for ( auto&& key : keys )
        {
            m_state_map.insert( std::pair{ key, glfw::input::ButtonState::k_released } );
        }

        m_handler_ptr->monitor( keys );
    }

    auto loggingPoll() const
    {
        std::stringstream ss;

        auto print = [ &ss ]( std::string key, auto info ) {
            ss << fmt::format( "Key: {}, State: {}\n", key, glfw::input::buttonStateToString( info.current ) );
            if ( info.hasBeenPressed() )
            {
                for ( auto i = 0; auto&& press : info.presses() )
                {
                    auto action_string = glfw::input::buttonActionToString( press.action );
                    ss << fmt::format( "Event [{}], State: {}\n", i++, action_string );
                }
            }
        };

        auto poll_result = m_handler_ptr->poll();

        for ( auto&& [ key, info ] : poll_result )
        {
            auto* name_cstr = glfwGetKeyName( key, 0 );

            if ( !name_cstr )
            {
                continue;
            }

            std::string_view key_name = name_cstr;
            print( std::string{ key_name }, info );
        }

        if ( auto msg = ss.str(); !msg.empty() )
        {
            spdlog::debug( msg );
        }

        return poll_result;
    }

    void update()
    {
        for ( auto&& [ key, info ] : loggingPoll() )
        {
            m_state_map[ key ] = info.current;
        }
    }

    glfw::input::ButtonState getState( glfw::input::KeyIndex key ) const
    {
        if ( auto found = m_state_map.find( key ); found != m_state_map.end() )
        {
            return found->second;
        }

        throw std::out_of_range{ "KeyboardStateTracker::getState(key): key not found" };
    }

    bool isPressed( glfw::input::KeyIndex key ) { return getState( key ) == glfw::input::ButtonState::k_pressed; }

  private:
    glfw::input::KeyboardHandler* m_handler_ptr;
    std::unordered_map<glfw::input::KeyIndex, glfw::input::ButtonState> m_state_map;
};

auto
pollMouseWithLog( glfw::input::MouseHandler& mouse )
{
    std::stringstream ss;
    auto mouse_poll = mouse.poll();

    auto print = [ &ss ]( std::string key, auto info ) {
        if ( info.hasBeenPressed() )
        {
            ss << fmt::format( "Key: {}, State: {}\n", key, glfw::input::buttonStateToString( info.current ) );
            for ( auto i = 0; auto&& press : info.presses() )
            {
                auto action_string = glfw::input::buttonActionToString( press.action );
                ss << fmt::format( "Event [{}], State: {}\n", i++, action_string );
            }
        }
    };

    for ( auto&& [ key, info ] : mouse_poll.buttons )
    {
        print( fmt::format( "Mouse button [{}]", key ), info );
    }

    auto [ x, y ] = mouse_poll.position;
    auto [ dx, dy ] = mouse_poll.movement;

    if ( dx != 0.0 || dy != 0.0 )
    {
        ss << fmt::format( "Mouse: position = [x = {}, y = {}]; movement = [dx = {}, dy = {}]\n", x, y, dx, dy );
    }

    if ( auto msg = ss.str(); !msg.empty() )
    {
        spdlog::debug( msg );
    }

    return mouse_poll;
}

vkwrap::Sampler
createTextureSampler( vk::PhysicalDevice physical_device, vk::Device logical_device )
{
    auto sampler_builder = vkwrap::SamplerBuilder{};
    auto anisotropy_supported = physical_device.getFeatures().samplerAnisotropy;

    sampler_builder.withMagFilter( vk::Filter::eNearest )
        .withMinFilter( vk::Filter::eLinear )
        .withAddressModeU( vk::SamplerAddressMode::eRepeat )
        .withAddressModeV( vk::SamplerAddressMode::eRepeat )
        .withAddressModeW( vk::SamplerAddressMode::eRepeat )
        .withAnisotropyEnable( anisotropy_supported )
        .withBorderColor( vk::BorderColor::eFloatOpaqueBlack )
        .withUnnormalizedCoordinates( VK_FALSE )
        .withCompareOp( vk::CompareOp::eAlways );

    return sampler_builder.make( logical_device, physical_device );
}

void
initializeIo( GLFWwindow* window )
{
    glfw::input::MouseHandler::instance( window ).setNormal();
    glfw::input::KeyboardHandler::instance( window );
}

auto
createKeyboardReader( GLFWwindow* window )
{
    auto keyboard = KeyboardStateTracker{ window };
    keyboard.monitor( std::array{
        GLFW_KEY_W,
        GLFW_KEY_A,
        GLFW_KEY_S,
        GLFW_KEY_D,
        GLFW_KEY_SPACE,
        GLFW_KEY_C,
        GLFW_KEY_Q,
        GLFW_KEY_E,
        GLFW_KEY_LEFT_ALT } );
    return keyboard;
}

auto
physicsLoop(
    vk::Extent2D extent,
    GLFWwindow* window,
    utils3d::Camera& camera,
    KeyboardStateTracker& keyboard,
    float delta_t // Time taken to render previous frame
)
{
    keyboard.update();
    auto& mouse = glfw::input::MouseHandler::instance( window );

    auto show_cursor = keyboard.isPressed( GLFW_KEY_LEFT_ALT );

    if ( !show_cursor && !ImGui::GetIO().WantCaptureMouse )
    {
        ImGui::SetMouseCursor( ImGuiMouseCursor_None );
        mouse.setHidden();
    } else
    {
        mouse.setNormal();
        mouse.poll();
    }

    const bool use_keyboard = !ImGui::GetIO().WantCaptureKeyboard;

    constexpr auto angular_per_delta_mouse = glm::radians( 0.1f );
    constexpr auto angular_per_delta_time = glm::radians( 25.0f );
    constexpr auto linear_per_delta_time = 5.0f;

    const auto calculate_movement =
        [ &keyboard, use_keyboard ]( glfw::input::KeyIndex plus, glfw::input::KeyIndex minus ) -> float {
        if ( !use_keyboard )
        {
            return 0.0f;
        }

        return 1.0f * static_cast<int>( keyboard.isPressed( plus ) ) -
            1.0f * static_cast<int>( keyboard.isPressed( minus ) );
    };

    const auto fwd_movement = calculate_movement( GLFW_KEY_W, GLFW_KEY_S );
    const auto side_movement = calculate_movement( GLFW_KEY_D, GLFW_KEY_A );
    const auto up_movement = calculate_movement( GLFW_KEY_SPACE, GLFW_KEY_C );

    const auto dir_movement =
        fwd_movement * camera.getDir() + side_movement * camera.getSideways() + up_movement * camera.getUp();

    const auto roll_movement = calculate_movement( GLFW_KEY_Q, GLFW_KEY_E );
    if ( glm::epsilonNotEqual( glm::length( dir_movement ), 0.0f, 0.05f ) )
    {
        camera.translate( glm::normalize( dir_movement ) * linear_per_delta_time * delta_t );
    }

    glm::quat yaw_rotation = glm::identity<glm::quat>(), pitch_rotation = glm::identity<glm::quat>();
    if ( !show_cursor )
    {
        auto mouse_events = pollMouseWithLog( mouse );
        auto [ dx, dy ] = mouse_events.movement;

        yaw_rotation = glm::angleAxis<float>( dx * angular_per_delta_mouse, camera.getUp() );
        pitch_rotation = glm::angleAxis<float>( dy * angular_per_delta_mouse, camera.getSideways() );
    }

    const auto roll_rotation =
        glm::angleAxis<float>( roll_movement * angular_per_delta_time * delta_t, camera.getDir() );
    const auto resulting_rotation = yaw_rotation * pitch_rotation * roll_rotation;
    camera.rotate( resulting_rotation );

    auto [ view, proj ] = camera.getMatrices( extent.width, extent.height );
    auto ubo = UniformBufferObject{ .model = glm::mat4x4{ 1.0f }, .view = view, .proj = proj, .origin_pos = {} };

    return ubo;
};

vk::UniqueDescriptorSetLayout
createDescriptorSetLayout( vk::Device logical_device )
{
    // create descriptor set layout
    const auto ubo_layout_binding = vk::DescriptorSetLayoutBinding{
        .binding = 0,
        .descriptorType = vk::DescriptorType::eUniformBuffer,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eVertex,
    };

    const auto sampler_layout_binding = vk::DescriptorSetLayoutBinding{
        .binding = 1,
        .descriptorType = vk::DescriptorType::eCombinedImageSampler,
        .descriptorCount = 1,
        .stageFlags = vk::ShaderStageFlagBits::eFragment,
        .pImmutableSamplers = nullptr };

    std::array<vk::DescriptorSetLayoutBinding, 2> bindings = { ubo_layout_binding, sampler_layout_binding };
    vk::DescriptorSetLayoutCreateInfo descriptor_set_layout_info{
        .bindingCount = static_cast<uint32_t>( bindings.size() ),
        .pBindings = bindings.data() };

    return logical_device.createDescriptorSetLayoutUnique( descriptor_set_layout_info );
}

std::vector<vk::UniqueDescriptorSet>
createAndUpdateDescriptorSets(
    vk::Device logical_device,
    vk::DescriptorSetLayout layout,
    vk::DescriptorPool pool,
    const std::vector<vkwrap::Buffer>& uniform_buffers,
    vk::Sampler sampler,
    vk::ImageView texture_view )
{
    auto layouts = std::vector<vk::DescriptorSetLayout>{ k_max_frames_in_flight, layout };

    auto alloc_info = vk::DescriptorSetAllocateInfo{
        .descriptorPool = pool,
        .descriptorSetCount = k_max_frames_in_flight,
        .pSetLayouts = layouts.data() };

    auto descriptor_sets = logical_device.allocateDescriptorSetsUnique( alloc_info );

    const auto image_info = vk::DescriptorImageInfo{
        .sampler = sampler,
        .imageView = texture_view,
        .imageLayout = vk::ImageLayout::eShaderReadOnlyOptimal };

    for ( auto i = uint32_t{ 0 }; i < k_max_frames_in_flight; ++i )
    {
        const auto buffer_info = vk::DescriptorBufferInfo{
            .buffer = uniform_buffers.at( i ).get(),
            .offset = 0,
            .range = sizeof( UniformBufferObject ) };

        const auto write_descriptor_sets = std::to_array<vk::WriteDescriptorSet>(
            { { .dstSet = descriptor_sets.at( i ).get(),
                .dstBinding = 0,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eUniformBuffer,
                .pBufferInfo = &buffer_info },
              { .dstSet = descriptor_sets.at( i ).get(),
                .dstBinding = 1,
                .dstArrayElement = 0,
                .descriptorCount = 1,
                .descriptorType = vk::DescriptorType::eCombinedImageSampler,
                .pImageInfo = &image_info } } );

        logical_device.updateDescriptorSets( write_descriptor_sets, {} );
    }

    return descriptor_sets;
}

struct GuiConfiguation
{
    bool draw_lines;
};

class MasterGui
{
  private:
    void drawConfigMenu()
    {
        ImGui::Begin( "Configuration" );
        ImGui::Checkbox( "Draw lines", &m_config.draw_lines );
        ImGui::End();
    }

  public:
    MasterGui( vk::Instance instance, vk::SurfaceKHR surface )
        : m_vkinfo_tab{ instance, surface }
    {
    }

    GuiConfiguation draw()
    {
        ImGui::ShowDemoWindow();
        m_vkinfo_tab.draw();
        drawConfigMenu();
        return m_config;
    }

  private:
    imgw::VulkanInformationTab m_vkinfo_tab;
    GuiConfiguation m_config;
};

constexpr auto k_subpass_dependency = vk::SubpassDependency{
    .srcSubpass = VK_SUBPASS_EXTERNAL,
    .dstSubpass = 0,
    .srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests,
    .dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput | vk::PipelineStageFlagBits::eEarlyFragmentTests,
    .srcAccessMask = {},
    .dstAccessMask = vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eDepthStencilAttachmentWrite,
};

constexpr auto k_pool_sizes = std::array{
    vk::DescriptorPoolSize{
        .type = vk::DescriptorType::eUniformBuffer,
        .descriptorCount = static_cast<uint32_t>( k_max_frames_in_flight ) },
    vk::DescriptorPoolSize{
        .type = vk::DescriptorType::eCombinedImageSampler,
        .descriptorCount = static_cast<uint32_t>( k_max_frames_in_flight ) } };

auto
meshChunks()
{
    auto mesher_future = std::async( std::launch::async, []() {
        chunk::ChunkMesher mesher;
        mesher.meshRenderArea();
        return mesher;
    } );

    return mesher_future;
}

struct PipelineCreateResult
{
    vkwrap::Pipeline pipeline;
    vk::UniquePipelineLayout layout;
};

PipelineCreateResult
createPipeline(
    vk::Device logical_device,
    vk::DescriptorSetLayout set_layout,
    vk::RenderPass render_pass,
    vk::PolygonMode mode )
{
    auto pipeline_layout = vkwrap::createPipelineLayout( logical_device, std::array{ set_layout } );

    auto vert_shader_module = vkwrap::ShaderModule{ "vertex_shader.spv", logical_device };
    auto frag_shader_module = vkwrap::ShaderModule{ "fragment_shader.spv", logical_device };
    auto vertex_info = chunk::ChunkMesher::getVertexInfo();

    auto pipeline_builder = vkwrap::DefaultPipelineBuilder{};
    auto pipeline = pipeline_builder.withVertexShader( vert_shader_module )
                        .withFragmentShader( frag_shader_module )
                        .withPipelineLayout( pipeline_layout.get() )
                        .withAttributeDescriptions( vertex_info.attribute_descr )
                        .withBindingDescriptions( vertex_info.binding_descr )
                        .withRenderPass( render_pass )
                        .withPolygonMode( mode )
                        .createPipeline( logical_device );

    return PipelineCreateResult{ std::move( pipeline ), std::move( pipeline_layout ) };
}

PipelineCreateResult
createLinePipeline( vk::Device logical_device, vk::DescriptorSetLayout set_layout, vk::RenderPass render_pass )
{
    auto pipeline_layout = vkwrap::createPipelineLayout( logical_device, std::array{ set_layout } );

    auto vert_shader_module = vkwrap::ShaderModule{ "vertex_shader.spv", logical_device };
    auto frag_shader_module = vkwrap::ShaderModule{ "fragment_shader.spv", logical_device };
    auto vertex_info = chunk::ChunkMesher::getVertexInfo();

    auto pipeline_builder = vkwrap::DefaultPipelineBuilder{};
    auto pipeline = pipeline_builder.withVertexShader( vert_shader_module )
                        .withFragmentShader( frag_shader_module )
                        .withPipelineLayout( pipeline_layout.get() )
                        .withAttributeDescriptions( vertex_info.attribute_descr )
                        .withBindingDescriptions( vertex_info.binding_descr )
                        .withRenderPass( render_pass )
                        .withPolygonMode( vk::PolygonMode::eLine )
                        .createPipeline( logical_device );

    return PipelineCreateResult{ std::move( pipeline ), std::move( pipeline_layout ) };
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

    auto mesher_future = meshChunks();

    auto glfw_instance = glfw::Instance{};
    auto window = glfw::wnd::Window{ glfw::wnd::WindowConfig{ .width = 1280, .height = 720, .title = "MinCraft" } };

    auto vk_instance = createInstance( glfw_instance, options.validation ).instance;
    auto surface = window.createSurface( vk_instance );
    auto swapchain_requirements = getSwapchainRequirements( surface.get(), options.uncapped_fps );
    auto physical_device = pickPhysicalDevice( vk_instance, swapchain_requirements );

    auto [ logical_device, graphics_queue, present_queue ] =
        createLogicalDeviceQueues( physical_device.get(), surface.get() );

    auto queues = std::array{ graphics_queue, present_queue };

    auto command_pool =
        vkwrap::CommandPool{ logical_device, graphics_queue, vk::CommandPoolCreateFlagBits::eResetCommandBuffer };

    auto one_time_cmd = vkwrap::OneTimeCommand{ command_pool, graphics_queue.get() };

    auto swapchain = createSwapchain(
        physical_device.get(),
        logical_device,
        surface.get(),
        graphics_queue,
        present_queue,
        swapchain_requirements );

    auto manager = vkwrap::Mman{
        k_vulkan_version,
        vk_instance,
        physical_device.get(),
        logical_device,
        graphics_queue.get(),
        command_pool.get() };

    auto depth_image = createDepthBuffer( swapchain, queues, manager );
    auto set_layout = createDescriptorSetLayout( logical_device );

    auto render_pass_builder = vkwrap::SimpleRenderPassBuilder{};
    auto render_pass = render_pass_builder.withSubpassDependencies( std::array{ k_subpass_dependency } )
                           .withColorAttachment( swapchain.getFormat() )
                           .withDepthAttachment( vk::Format::eD32Sfloat )
                           .make( logical_device );

    auto fill_pipeline = createPipeline( logical_device, set_layout.get(), render_pass.get(), vk::PolygonMode::eFill );
    auto line_pipeline = createPipeline( logical_device, set_layout.get(), render_pass.get(), vk::PolygonMode::eLine );

    auto framebuffers = createFramebuffers( swapchain, depth_image.getView(), logical_device, render_pass.get() );
    auto render_infos = createRenderInfos( logical_device, command_pool, queues, manager );

    initializeIo( window ); // Initialize GLFW IO before creating ImGui to keep the callbacks, because the backend ImGui
                            // has for GLFW overrides the callbacks and saves those that were previously set.

    auto imgui_resources = imgw::ImGuiResources{ imgw::ImGuiResources::ImGuiResourcesInitInfo{
        .instance = vk_instance,
        .window = window.get(),
        .physical_device = physical_device.get(),
        .logical_device = logical_device,
        .graphics = graphics_queue,
        .swapchain = swapchain,
        .upload_context = one_time_cmd,
        .render_pass = render_pass.get() } };

    auto sampler = createTextureSampler( physical_device.get(), logical_device );
    auto texture_image = createTextureImage( queues, manager, "texture.ktx2" );
    auto descriptor_pool = vkwrap::createDescriptorPool( logical_device, k_pool_sizes );

    auto descriptor_sets = createAndUpdateDescriptorSets(
        logical_device,
        set_layout.get(),
        descriptor_pool.get(),
        render_infos.uniform_buffers,
        sampler.get(),
        texture_image.getView() );

    auto recreate_swapchain_wrapped = [ &, &logical_device = logical_device ]() {
        logical_device->waitIdle();
        swapchain.recreate();
        depth_image = createDepthBuffer( swapchain, queues, manager );
        framebuffers = createFramebuffers( swapchain, depth_image.getView(), logical_device, render_pass.get() );
    };

    auto mesher = mesher_future.get();
    auto vertex_buffer = createVertexBuffer( queues, mesher, manager );
    auto index_buffer = createIndexBuffer( queues, mesher, manager );

    struct RenderConfig
    {
        UniformBufferObject ubo;
        bool draw_lines;
    };

    uint32_t current_frame = 0;

    auto fill_command_buffer =
        [ & ]( vk::CommandBuffer& cmd, uint32_t image_index, vk::Extent2D extent, RenderConfig config ) {
            const auto clear_values = std::array{
                vk::ClearValue{ .color = { utils::hexToRGBA( 0x181818ff ) } },
                vk::ClearValue{ .depthStencil = { .depth = 1.0f, .stencil = 0 } } };

            const auto render_pass_info = vk::RenderPassBeginInfo{
                .renderPass = render_pass.get(),
                .framebuffer = framebuffers.at( image_index ).get(),
                .renderArea = { vk::Offset2D{ 0, 0 }, extent },
                .clearValueCount = static_cast<uint32_t>( clear_values.size() ),
                .pClearValues = clear_values.data() };

            cmd.reset();
            cmd.begin( vk::CommandBufferBeginInfo{ .flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse } );
            cmd.beginRenderPass( render_pass_info, vk::SubpassContents::eInline );

            cmd.bindPipeline(
                vk::PipelineBindPoint::eGraphics,
                ( config.draw_lines ? line_pipeline.pipeline : fill_pipeline.pipeline ) );

            cmd.bindVertexBuffers( 0, vertex_buffer.get(), vk::DeviceSize{ 0 } );
            cmd.bindIndexBuffer( index_buffer.get(), 0, chunk::ChunkMesher::k_index_type );

            const auto [ width, height ] = extent;

            // Negative viewport coordinates. This is quite legal and well-formed. See
            // https://registry.khronos.org/vulkan/specs/1.3-extensions/man/html/VK_KHR_maintenance1.html
            // This is used to flip the coordinate system without modifying the transformation matrices
            const auto viewport = vk::Viewport{
                .x = 0.0f,
                .y = static_cast<float>( height ),
                .width = static_cast<float>( width ),
                .height = -static_cast<float>( height ),
                .minDepth = 0.0f,
                .maxDepth = 1.0f };

            const auto scissor = vk::Rect2D{ .offset = { 0, 0 }, .extent = extent };

            cmd.setViewport( 0, viewport );
            cmd.setScissor( 0, scissor );

            cmd.bindDescriptorSets(
                vk::PipelineBindPoint::eGraphics,
                ( config.draw_lines ? line_pipeline.layout : fill_pipeline.layout ).get(),
                0,
                descriptor_sets.at( current_frame ).get(),
                {} );

            cmd.drawIndexed( static_cast<uint32_t>( mesher.getIndicesCount() ), 1, 0, 0, 0 );

            imgui_resources.fillCommandBuffer( cmd );

            cmd.endRenderPass();
            cmd.end();
        };

    auto camera = utils3d::Camera{ glm::vec3{ 0.0f, 0.0f, 15.0f } };
    auto keyboard = createKeyboardReader( window );
    auto prev_timepoint = std::chrono::high_resolution_clock::now();

    auto gui = MasterGui{ vk_instance.get(), surface.get() };

    auto application_loop = [ &gui, &mesher, &camera, &window, &keyboard, &prev_timepoint ]( vk::Extent2D extent ) {
        auto curr_timepoint = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> delta_time = curr_timepoint - prev_timepoint;
        prev_timepoint = curr_timepoint;

        auto config = gui.draw(); // Get configuration and pass it to physicsLoop; TODO [Sergei]
        auto ubo = physicsLoop( extent, window, camera, keyboard, delta_time.count() );

        auto [ x, y ] = mesher.getRenderAreaRight();
        ubo.origin_pos = glm::vec2{ x, y };

        return RenderConfig{ ubo, config.draw_lines };
    };

    auto render_frame = [ &,
                          &logical_device = logical_device,
                          &graphics_queue = graphics_queue,
                          &present_queue = present_queue ]( RenderConfig config ) {
        auto& current_frame_data = render_infos.sync_primitives.at( current_frame );
        auto& command_buffer = render_infos.imgui_command_buffers.at( current_frame );
        logical_device->waitForFences( current_frame_data.in_flight_fence.get(), VK_TRUE, UINT64_MAX );

        const auto extent = swapchain.getExtent();
        auto& uniform_buffer = render_infos.uniform_buffers.at( current_frame );
        uniform_buffer.update( config.ubo, sizeof( UniformBufferObject ) );

        auto [ acquire_result, image_index ] =
            swapchain.acquireNextImage( current_frame_data.image_availible_semaphore.get() );

        if ( acquire_result == vk::Result::eErrorOutOfDateKHR )
        {
            recreate_swapchain_wrapped();
            return;
        }

        fill_command_buffer( command_buffer.get(), image_index, extent, config );

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

        vk::Result result_present = present_queue.presentKHRWithOutOfDate(
            swapchain.get(),
            current_frame_data.render_finished_semaphore.get(),
            image_index );

        if ( shouldRecreateSwapchain( result_present ) )
        {
            recreate_swapchain_wrapped();
        }

        current_frame = ( current_frame + 1 ) % k_max_frames_in_flight;
    };

    auto draw_loop = [ & ]() {
        imgui_resources.newFrame();
        auto ubo = application_loop( swapchain.getExtent() );
        imgui_resources.renderFrame();
        render_frame( ubo );
    };

    auto renderer_thread = std::jthread{ [ &, &logical_device = logical_device ]( std::stop_token stop ) {
        while ( !stop.stop_requested() )
        {
            draw_loop();
        }
        logical_device->waitIdle(); // To shut down nicely
    } };

    while ( window.running() )
    {
        glfwPollEvents();
    }
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
