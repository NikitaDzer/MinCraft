#pragma once

#include "vkwrap/render_pass.h"
#include "vkwrap/shader_module.h"

#include <range/v3/range/concepts.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/all.hpp>

namespace vkwrap
{

// Contains classes that configure Vulkan Pipeline class

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
    Derived& withPolygonMode( vk::PolygonMode mode ) &
    {
        m_polygon_mode = mode;
        return static_cast<Derived&>( *this );
    }

    void make( vk::GraphicsPipelineCreateInfo& pipeline_create_info )
    {
        m_rasterizer.setDepthClampEnable( VK_FALSE );
        m_rasterizer.setRasterizerDiscardEnable( VK_FALSE );
        m_rasterizer.setPolygonMode( m_polygon_mode );
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
    vk::PolygonMode m_polygon_mode = vk::PolygonMode::eFill;
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
    Derived& withPipelineLayout( vk::PipelineLayout layout ) &
    {
        m_pipeline_layout = layout;
        return static_cast<Derived&>( *this );
    }

    void make( vk::GraphicsPipelineCreateInfo& pipeline_create_info )
    {
        pipeline_create_info.setLayout( m_pipeline_layout ); //
    }

  protected:
    PipelineLayoutCfg() = default;
    PipelineLayoutCfg( const PipelineLayoutCfg& ) = default;
    PipelineLayoutCfg& operator=( const PipelineLayoutCfg& ) = default;
    PipelineLayoutCfg& operator=( PipelineLayoutCfg&& ) = default;
    PipelineLayoutCfg( PipelineLayoutCfg&& ) = default;
    ~PipelineLayoutCfg() = default;

  private:
    vk::PipelineLayout m_pipeline_layout;
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
    Derived& withRenderPass( vk::RenderPass render_pass ) &
    {
        m_render_pass = render_pass;
        return static_cast<Derived&>( *this );
    }

    vk::RenderPass getRenderPass() const { return m_render_pass; }

    void make( vk::GraphicsPipelineCreateInfo& pipeline_create_info )
    {
        pipeline_create_info.setRenderPass( getRenderPass() );
        pipeline_create_info.setSubpass( 0 );
    }

  protected:
    RenderPassCfg() = default;
    RenderPassCfg( RenderPassCfg&& ) = default;
    RenderPassCfg& operator=( RenderPassCfg&& ) = default;
    RenderPassCfg( const RenderPassCfg& ) = default;
    RenderPassCfg& operator=( const RenderPassCfg& ) = default;
    ~RenderPassCfg() = default;

  private:
    vk::RenderPass m_render_pass;
};

}; // namespace cfgs

} // namespace vkwrap
