#pragma once

#include "common/vulkan_include.h"

#include "glfw/window.h"

#include "vkwrap/command.h"
#include "vkwrap/descriptors.h"
#include "vkwrap/queues.h"

#include "vkwrap/swapchain.h"

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>

#include <utility>

namespace imgw
{

struct ImGuiResourceFlagType
{
};

constexpr auto ImGuiResourceFlag = ImGuiResourceFlagType{}; // Tag type

class ImGuiUniqueLibraryResource
{
  public:
    ImGuiUniqueLibraryResource() = default;
    explicit ImGuiUniqueLibraryResource( ImGuiResourceFlagType )
        : m_initialized{ true }
    {
    }

  public:
    void swap( ImGuiUniqueLibraryResource& rhs ) { std::swap( m_initialized, rhs.m_initialized ); }

    ImGuiUniqueLibraryResource( const ImGuiUniqueLibraryResource& ) = delete;
    ImGuiUniqueLibraryResource& operator=( const ImGuiUniqueLibraryResource& ) = delete;

    ImGuiUniqueLibraryResource( ImGuiUniqueLibraryResource&& rhs ) noexcept { swap( rhs ); }
    ImGuiUniqueLibraryResource& operator=( ImGuiUniqueLibraryResource&& rhs ) noexcept
    {
        swap( rhs );
        return *this;
    }

    ~ImGuiUniqueLibraryResource()
    {
        if ( !m_initialized )
        {
            return;
        }

        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

  private:
    bool m_initialized = false;
};

constexpr uint32_t k_default_descriptor_count = 1000;

constexpr auto k_imgui_pool_sizes = std::to_array<vk::DescriptorPoolSize>(
    { { vk::DescriptorType::eSampler, k_default_descriptor_count },
      { vk::DescriptorType::eCombinedImageSampler, k_default_descriptor_count },
      { vk::DescriptorType::eSampledImage, k_default_descriptor_count },
      { vk::DescriptorType::eStorageImage, k_default_descriptor_count },
      { vk::DescriptorType::eUniformTexelBuffer, k_default_descriptor_count },
      { vk::DescriptorType::eStorageTexelBuffer, k_default_descriptor_count },
      { vk::DescriptorType::eUniformBuffer, k_default_descriptor_count },
      { vk::DescriptorType::eStorageBuffer, k_default_descriptor_count },
      { vk::DescriptorType::eUniformBufferDynamic, k_default_descriptor_count },
      { vk::DescriptorType::eStorageBufferDynamic, k_default_descriptor_count },
      { vk::DescriptorType::eInputAttachment, k_default_descriptor_count } } );

class ImGuiResources
{
  private:
    static void checkVkResult( VkResult c_res )
    {
        auto result = vk::Result{ c_res };
        auto error_message = vk::to_string( result );
        vk::resultCheck( result, error_message.c_str() );
    }

  public:
    void newFrame()
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void renderFrame() { ImGui::Render(); }

    void fillCommandBuffer( vk::CommandBuffer& cmd ) { ImGui_ImplVulkan_RenderDrawData( ImGui::GetDrawData(), cmd ); }

  private:
    ImGuiResourceFlagType initializeLibrary(
        vk::Instance instance,
        GLFWwindow* window,
        vk::PhysicalDevice physical_device,
        vk::Device logical_device,
        vkwrap::Queue graphics,
        const vkwrap::Swapchain& swapchain,
        vkwrap::OneTimeCommand& upload_context,
        vk::RenderPass render_pass )
    {
        assert( window );

        // Step 1. Initialize library for glfw and misc imgui stuff
        IMGUI_CHECKVERSION(); // Verify that compiled imgui binary matches the header
        ImGui::CreateContext();
        ImGui_ImplGlfw_InitForVulkan( window, true );

        // Step 2. Heavy duty Vulkan stuff
        auto info = ImGui_ImplVulkan_InitInfo{
            .Instance = instance,
            .PhysicalDevice = physical_device,
            .Device = logical_device,
            .QueueFamily = graphics.familyIndex(),
            .Queue = graphics.get(),
            .PipelineCache = VK_NULL_HANDLE,
            .DescriptorPool = *m_descriptor_pool,
            .Subpass = 0,
            .MinImageCount = swapchain.getImagesCount(),
            .ImageCount = swapchain.getImagesCount(),
            .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
            .Allocator = nullptr,
            .CheckVkResultFn = checkVkResult };

        ImGui_ImplVulkan_Init( &info, render_pass );

        // Step 3. Upload fonts to the GPU
        upload_context.submitAndWait( []( vk::CommandBuffer& cmd ) { ImGui_ImplVulkan_CreateFontsTexture( cmd ); } );

        return ImGuiResourceFlag;
    }

  public:
    struct ImGuiResourcesInitInfo
    {
        vk::Instance instance;
        GLFWwindow* window = nullptr;
        vk::PhysicalDevice physical_device;
        vk::Device logical_device;
        vkwrap::Queue graphics;
        const vkwrap::Swapchain& swapchain;
        vkwrap::OneTimeCommand& upload_context;
        vk::RenderPass render_pass;
    };

    explicit ImGuiResources( ImGuiResourcesInitInfo info )
        : m_descriptor_pool{ info.logical_device, k_imgui_pool_sizes },
          m_library_resource{ initializeLibrary(
              info.instance,
              info.window,
              info.physical_device,
              info.logical_device,
              info.graphics,
              info.swapchain,
              info.upload_context,
              info.render_pass ) }
    {
    }

  private:
    vkwrap::DescriptorPool m_descriptor_pool;
    ImGuiUniqueLibraryResource m_library_resource;
};

}; // namespace imgw