#pragma once

#include "vkwrap/shader_module.h"
#include <range/v3/range.hpp>
#include <range/v3/view.hpp>

namespace vkwrap
{
// contains classes that configure Vulkan Pipeline class
namespace cfgs
{

// [krisszzzz] All cfgs classes uses as "Strategy" pattern
// Unfornately, we have to use public inheritence in this case
// In our case the public inheritence harmful, because of polymorphism
// To ensure correct working with this classes the functions of "rules of 5"
// and the constructor should be written as protected function members

template <typename Derived> class ShaderCfg
{
  public:
    Derived& withVertexShader( const vkwrap::ShaderModule& shader_module ) &
    {
        constexpr auto vertex_shader = 0;
        m_shader_stages[ vertex_shader ].setStage( vk::ShaderStageFlagBits::eVertex );
        m_shader_stages[ vertex_shader ].setModule( shader_module );
        m_shader_stages[ vertex_shader ].setPName( "main" );

        return static_cast<Derived&>( *this );
    }

    Derived& withBindingDescriptions( auto&& binding_descr ) &
    {
        m_vertex_input_info.setVertexBindingDescriptionCount( static_cast<uint32_t>( binding_descr.size() ) );
        m_vertex_input_info.setPVertexBindingDescriptions( binding_descr.data() );

        return static_cast<Derived&>( *this );
    };

    Derived& withAttributeDescriptions( auto&& attribute_descr ) &
    {
        m_vertex_input_info.setVertexAttributeDescriptionCount( static_cast<uint32_t>( attribute_descr.size() ) );
        m_vertex_input_info.setPVertexAttributeDescriptions( attribute_descr.data() );

        return static_cast<Derived&>( *this );
    }

    Derived& withFragmentShader( const vkwrap::ShaderModule& shader_module ) &
    {
        constexpr auto fragment_shader = 1;
        m_shader_stages[ fragment_shader ].setStage( vk::ShaderStageFlagBits::eFragment );
        m_shader_stages[ fragment_shader ].setModule( shader_module );
        m_shader_stages[ fragment_shader ].setPName( "main" );

        return static_cast<Derived&>( *this );
    }

    void make( vk::GraphicsPipelineCreateInfo& pipeline_create_info )
    {
        pipeline_create_info.setStageCount( stages_count );
        pipeline_create_info.setPStages( m_shader_stages.data() );
        pipeline_create_info.setPVertexInputState( &m_vertex_input_info );
    }

  protected:
    ShaderCfg() = default;
    ShaderCfg( ShaderCfg&& ) = default;
    ShaderCfg( const ShaderCfg& ) = default;
    ShaderCfg& operator=( const ShaderCfg& ) = default;
    ShaderCfg& operator=( ShaderCfg&& ) = default;
    ~ShaderCfg() = default;

  private:
    constexpr static auto stages_count = 2;

  private:
    std::array<vk::PipelineShaderStageCreateInfo, stages_count> m_shader_stages;
    vk::PipelineVertexInputStateCreateInfo m_vertex_input_info;
};

template <typename Derived> class MultisamplingCfg
{
  public:
    void make( vk::GraphicsPipelineCreateInfo& pipeline_create_info )
    {
        m_multisampling.setRasterizationSamples( vk::SampleCountFlagBits::e1 );
        m_multisampling.setSampleShadingEnable( VK_FALSE );
        m_multisampling.setMinSampleShading( 1.0f );
        m_multisampling.setPSampleMask( nullptr );
        m_multisampling.setAlphaToCoverageEnable( VK_FALSE );
        m_multisampling.setAlphaToOneEnable( VK_FALSE );

        pipeline_create_info.setPMultisampleState( &m_multisampling );
    }

  protected:
    MultisamplingCfg() = default;
    MultisamplingCfg( MultisamplingCfg&& ) = default;
    MultisamplingCfg( const MultisamplingCfg& ) = default;
    MultisamplingCfg& operator=( const MultisamplingCfg& ) = default;
    MultisamplingCfg& operator=( MultisamplingCfg&& ) = default;
    ~MultisamplingCfg() = default;

  private:
    vk::PipelineMultisampleStateCreateInfo m_multisampling;
};

template <typename Derived> class ViewportScissorCfg
{
  public:
    void make( vk::GraphicsPipelineCreateInfo& pipeline_create_info )
    {
        pipeline_create_info.setPViewportState( &m_viewport_state );
        pipeline_create_info.setPDynamicState( &m_dynamic_state_info );
    }

  protected:
    ViewportScissorCfg()
    {
        m_viewport_state.setViewportCount( 1 );
        m_viewport_state.setScissorCount( 1 );

        m_dynamic_states[ 0 ] = vk::DynamicState::eViewport;
        m_dynamic_states[ 1 ] = vk::DynamicState::eScissor;

        m_dynamic_state_info.setDynamicStateCount( states_count );
        m_dynamic_state_info.setDynamicStates( m_dynamic_states );
    }

    ViewportScissorCfg( ViewportScissorCfg&& ) = default;
    ViewportScissorCfg( const ViewportScissorCfg& ) = default;
    ViewportScissorCfg& operator=( const ViewportScissorCfg& ) = default;
    ViewportScissorCfg& operator=( ViewportScissorCfg&& ) = default;
    ~ViewportScissorCfg() = default;

  private:
    constexpr static auto states_count = 2;

  private:
    std::array<vk::DynamicState, states_count> m_dynamic_states;
    vk::PipelineDynamicStateCreateInfo m_dynamic_state_info;
    vk::PipelineViewportStateCreateInfo m_viewport_state;
};

template <typename Derived> class RasterizerCfg
{
  public:
    void make( vk::GraphicsPipelineCreateInfo& pipeline_create_info )
    {
        m_rasterizer.setDepthClampEnable( VK_FALSE );
        m_rasterizer.setRasterizerDiscardEnable( VK_FALSE );
        m_rasterizer.setPolygonMode( vk::PolygonMode::eFill );
        m_rasterizer.setLineWidth( 1.0f );
        m_rasterizer.setCullMode( vk::CullModeFlagBits::eBack );
        m_rasterizer.setFrontFace( vk::FrontFace::eCounterClockwise );
        m_rasterizer.setDepthBiasEnable( VK_FALSE );
        m_rasterizer.setDepthBiasConstantFactor( 0.0f );
        m_rasterizer.setDepthBiasClamp( 0.0f );
        m_rasterizer.setDepthBiasSlopeFactor( 0.0f );

        pipeline_create_info.setPRasterizationState( &m_rasterizer );
    }

  protected:
    RasterizerCfg() = default;
    RasterizerCfg( RasterizerCfg&& ) = default;
    RasterizerCfg( const RasterizerCfg& ) = default;
    RasterizerCfg& operator=( const RasterizerCfg& ) = default;
    RasterizerCfg& operator=( RasterizerCfg&& ) = default;
    ~RasterizerCfg() = default;

  private:
    vk::PipelineRasterizationStateCreateInfo m_rasterizer;
};

template <typename Derived> class InputAssemblyCfg
{
  public:
    void make( vk::GraphicsPipelineCreateInfo& pipeline_create_info )
    {
        m_input_assembly.setTopology( vk::PrimitiveTopology::eTriangleList );
        m_input_assembly.setPrimitiveRestartEnable( VK_FALSE );

        pipeline_create_info.setPInputAssemblyState( &m_input_assembly );
    }

  protected:
    InputAssemblyCfg() = default;
    InputAssemblyCfg( InputAssemblyCfg&& ) = default;
    InputAssemblyCfg( const InputAssemblyCfg& ) = default;
    InputAssemblyCfg& operator=( const InputAssemblyCfg& ) = default;
    InputAssemblyCfg& operator=( InputAssemblyCfg&& ) = default;
    ~InputAssemblyCfg() = default;

  private:
    vk::PipelineInputAssemblyStateCreateInfo m_input_assembly;
};

template <typename Derived> class PipelineLayoutCfg
{
  public:
    template <typename Layout> Derived& withPipelineLayout( vk::Device device, Layout&& layouts = {} ) &
    {
        m_layouts = ranges::views::all( layouts ) | ranges::to_vector;

        vk::PipelineLayoutCreateInfo layout_create_info{
            .setLayoutCount = static_cast<uint32_t>( m_layouts.size() ),
            .pSetLayouts = m_layouts.data() };

        m_pipeline_layout = device.createPipelineLayoutUnique( layout_create_info );

        return static_cast<Derived&>( *this );
    }

    void make( vk::GraphicsPipelineCreateInfo& pipeline_create_info )
    {
        pipeline_create_info.setLayout( m_pipeline_layout.get() ); //
    }

    vk::PipelineLayout getPipelineLayout() const { return m_pipeline_layout.get(); }

  protected:
    PipelineLayoutCfg() = default;
    // deleted because of vk::UniquePipelinelayout
    PipelineLayoutCfg( const PipelineLayoutCfg& ) = delete;
    PipelineLayoutCfg& operator=( const PipelineLayoutCfg& ) = delete;

    PipelineLayoutCfg& operator=( PipelineLayoutCfg&& ) = default;
    PipelineLayoutCfg( PipelineLayoutCfg&& ) = default;
    ~PipelineLayoutCfg() = default;

  private:
    std::vector<vk::DescriptorSetLayout> m_layouts;
    vk::UniquePipelineLayout m_pipeline_layout;
};

template <typename Derived> class BlendStateCfg
{
  public:
    void make( vk::GraphicsPipelineCreateInfo& pipeline_create_info )
    {
        // clang-format off
        m_color_blend_attachment.setColorWriteMask(
	    vk::ColorComponentFlagBits::eA |
	    vk::ColorComponentFlagBits::eR |
	    vk::ColorComponentFlagBits::eG |
	    vk::ColorComponentFlagBits::eB );
        // clang-format on

        m_color_blend_attachment.setBlendEnable( VK_FALSE );
        m_color_blending.setLogicOpEnable( VK_FALSE );
        m_color_blending.setAttachmentCount( 1 );
        m_color_blending.setPAttachments( &m_color_blend_attachment );

        pipeline_create_info.setPColorBlendState( &m_color_blending );
    }

  protected:
    BlendStateCfg() = default;
    BlendStateCfg( BlendStateCfg&& ) = default;
    BlendStateCfg( const BlendStateCfg& ) = default;
    BlendStateCfg& operator=( const BlendStateCfg& ) = default;
    BlendStateCfg& operator=( BlendStateCfg&& ) = default;
    ~BlendStateCfg() = default;

  private:
    vk::PipelineColorBlendAttachmentState m_color_blend_attachment;
    vk::PipelineColorBlendStateCreateInfo m_color_blending;
};

template <typename Derived> class DepthStencilStateCfg
{

  public:
    void make( vk::GraphicsPipelineCreateInfo& pipeline_create_info )
    {
        pipeline_create_info.setPDepthStencilState( &m_depth_stencil );
    }

  protected:
    DepthStencilStateCfg()
    {
        m_depth_stencil.setDepthTestEnable( VK_TRUE );
        m_depth_stencil.setDepthWriteEnable( VK_TRUE );
        m_depth_stencil.setDepthCompareOp( vk::CompareOp::eLess );
        m_depth_stencil.setDepthBoundsTestEnable( VK_FALSE );
        m_depth_stencil.setStencilTestEnable( VK_FALSE );
    }

  private:
    vk::PipelineDepthStencilStateCreateInfo m_depth_stencil;
};

template <typename Derived> class RenderPassCfg
{
  public:
    Derived& withColorAttachment( vk::Format swap_chain_format ) &
    {
        m_color_attachment.setFormat( swap_chain_format );
        m_color_attachment.setSamples( vk::SampleCountFlagBits::e1 );
        m_color_attachment.setLoadOp( vk::AttachmentLoadOp::eClear );
        m_color_attachment.setStoreOp( vk::AttachmentStoreOp::eStore );
        m_color_attachment.setStencilLoadOp( vk::AttachmentLoadOp::eDontCare );
        m_color_attachment.setStencilStoreOp( vk::AttachmentStoreOp::eDontCare );
        m_color_attachment.setInitialLayout( vk::ImageLayout::eUndefined );
        m_color_attachment.setFinalLayout( vk::ImageLayout::ePresentSrcKHR );

        return static_cast<Derived&>( *this );
    };

    Derived& withDepthAttachment( vk::Format depth_format ) &
    {
        m_depth_attachment.setFormat( depth_format );
        m_depth_attachment.setSamples( vk::SampleCountFlagBits::e1 );
        m_depth_attachment.setLoadOp( vk::AttachmentLoadOp::eClear );
        m_depth_attachment.setStoreOp( vk::AttachmentStoreOp::eDontCare );
        m_depth_attachment.setStencilLoadOp( vk::AttachmentLoadOp::eDontCare );
        m_depth_attachment.setStencilStoreOp( vk::AttachmentStoreOp::eDontCare );
        m_depth_attachment.setInitialLayout( vk::ImageLayout::eUndefined );
        m_depth_attachment.setFinalLayout( vk::ImageLayout::eDepthStencilAttachmentOptimal );

        return static_cast<Derived&>( *this );
    }

    template <typename SubpassDep> Derived& withSubpassDependencies( SubpassDep&& dependecies )
    {
        m_subpass_dependecies = ranges::views::all( dependecies ) | ranges::to_vector;

        return static_cast<Derived&>( *this );
    }

    Derived& withRenderPass( vk::Device device ) &
    {
        vk::AttachmentReference color_attachment_ref{
            .attachment = 0,
            .layout = vk::ImageLayout::eColorAttachmentOptimal };

        vk::AttachmentReference depth_attachment_ref{
            .attachment = 1,
            .layout = vk::ImageLayout::eDepthStencilAttachmentOptimal };

        vk::SubpassDescription subpass{
            .pipelineBindPoint = vk::PipelineBindPoint::eGraphics,
            .colorAttachmentCount = 1,
            .pColorAttachments = &color_attachment_ref,
            .pDepthStencilAttachment = &depth_attachment_ref };

        auto attachments = std::array{ m_color_attachment, m_depth_attachment };

        vk::RenderPassCreateInfo render_pass_create_info{
            .attachmentCount = static_cast<uint32_t>( attachments.size() ),
            .pAttachments = attachments.data(),
            .subpassCount = 1,
            .pSubpasses = &subpass,
            .dependencyCount = static_cast<uint32_t>( m_subpass_dependecies.size() ),
            .pDependencies = m_subpass_dependecies.data() };

        m_render_pass = device.createRenderPassUnique( render_pass_create_info );

        return static_cast<Derived&>( *this );
    }

    vk::RenderPass getRenderPass() const { return m_render_pass.get(); }

    void make( vk::GraphicsPipelineCreateInfo& pipeline_create_info )
    {
        pipeline_create_info.setRenderPass( getRenderPass() );
        pipeline_create_info.setSubpass( 0 );
    }

  protected:
    RenderPassCfg() = default;
    RenderPassCfg( RenderPassCfg&& ) = default;
    RenderPassCfg& operator=( RenderPassCfg&& ) = default;
    // deleted because of vk::UniqueRenderPass
    RenderPassCfg( const RenderPassCfg& ) = delete;
    RenderPassCfg& operator=( const RenderPassCfg& ) = delete;
    ~RenderPassCfg() = default;

  private:
    std::vector<vk::SubpassDependency> m_subpass_dependecies;
    vk::AttachmentDescription m_color_attachment;
    vk::AttachmentDescription m_depth_attachment;
    vk::UniqueRenderPass m_render_pass;
};

}; // namespace cfgs

} // namespace vkwrap
