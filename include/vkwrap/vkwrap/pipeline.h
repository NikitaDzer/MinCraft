#pragma once

#include "vkwrap/core.h"
#include "vkwrap/shader_module.h"
#include <cassert>
#include <type_traits>

namespace vkwrap
{
// clang-format off
template<typename T>
concept PipelineCfg = requires( T t, vk::GraphicsPipelineCreateInfo* create_info )
{
    { t.make( create_info ) } -> std::convertible_to<void>;
};
// clang-format on

// contains classes that configure Vulkan Pipeline class
namespace cfgs
{
class ShaderCfg
{
  public:
    void setVertexShader( vkwrap::ShaderModule& shader_module )
    {
        constexpr auto vertex_shader = 0;
        shader_stages[ vertex_shader ].setStage( vk::ShaderStageFlagBits::eVertex );
        shader_stages[ vertex_shader ].setModule( shader_module );
        shader_stages[ vertex_shader ].setPName( "main" );
    }

    void setFragmentShader( vkwrap::ShaderModule& shader_module )
    {
        constexpr auto fragment_shader = 1;
        shader_stages[ fragment_shader ].setStage( vk::ShaderStageFlagBits::eFragment );
        shader_stages[ fragment_shader ].setModule( shader_module );
        shader_stages[ fragment_shader ].setPName( "main" );
    }

    void make( vk::GraphicsPipelineCreateInfo* create_info )
    {
        create_info->setStageCount( stages_count );
        create_info->setPStages( shader_stages );
    }

  private:
    constexpr static auto stages_count = 2;

  private:
    vk::PipelineShaderStageCreateInfo shader_stages[ stages_count ];
};

class MultisamplingCfg
{
  public:
    MultisamplingCfg()
    {
        multisampling.setRasterizationSamples( vk::SampleCountFlagBits::e1 );
        multisampling.setSampleShadingEnable( VK_FALSE );
        multisampling.setMinSampleShading( 1.0f );
        multisampling.setPSampleMask( nullptr );
        multisampling.setAlphaToCoverageEnable( VK_FALSE );
        multisampling.setAlphaToOneEnable( VK_FALSE );
    }

    // clang-format off
    void make( vk::GraphicsPipelineCreateInfo* create_info )
    {
	create_info->setPMultisampleState( &multisampling );
    }
    // clang-format on

  private:
    vk::PipelineMultisampleStateCreateInfo multisampling;
};

class ViewportScissorCfg
{
  public:
    ViewportScissorCfg()
    {
        viewport_state.setViewportCount( 1 );
        viewport_state.setScissorCount( 1 );

        dynamic_states[ 0 ] = vk::DynamicState::eViewport;
        dynamic_states[ 1 ] = vk::DynamicState::eScissor;

        dynamic_state.setDynamicStateCount( states_count );
        dynamic_state.setDynamicStates( dynamic_states );
    }

    void make( vk::GraphicsPipelineCreateInfo* create_info )
    {
        create_info->setPViewportState( &viewport_state );
        create_info->setPDynamicState( &dynamic_state );
    }

  private:
    constexpr static auto states_count = 2;

  private:
    vk::DynamicState dynamic_states[ states_count ];
    vk::PipelineDynamicStateCreateInfo dynamic_state;
    vk::PipelineViewportStateCreateInfo viewport_state;
};

class RasterizerCfg
{
  public:
    RasterizerCfg()
    {
        rasterizer.setDepthClampEnable( VK_FALSE );
        rasterizer.setRasterizerDiscardEnable( VK_FALSE );
        rasterizer.setPolygonMode( vk::PolygonMode::eFill );
        rasterizer.setLineWidth( 1.0f );
        rasterizer.setCullMode( vk::CullModeFlagBits::eBack );
        rasterizer.setFrontFace( vk::FrontFace::eClockwise );
        rasterizer.setDepthBiasEnable( VK_FALSE );
        rasterizer.setDepthBiasConstantFactor( 0.0f );
        rasterizer.setDepthBiasClamp( 0.0f );
        rasterizer.setDepthBiasSlopeFactor( 0.0f );
    }

    // clang-format off
    void make( vk::GraphicsPipelineCreateInfo* create_info )
    {
	create_info->setPRasterizationState( &rasterizer );
    }
    // clang-format on

  private:
    vk::PipelineRasterizationStateCreateInfo rasterizer;
};

class InputAssemblyCfg
{
  public:
    InputAssemblyCfg()
    {
        input_assembly.setTopology( vk::PrimitiveTopology::eTriangleList );
        input_assembly.setPrimitiveRestartEnable( VK_FALSE );
    }

    // clang-format off
    void make( vk::GraphicsPipelineCreateInfo* create_info )
    {
	create_info->setPInputAssemblyState( &input_assembly );
    }
    // clang-format on

  private:
    vk::PipelineInputAssemblyStateCreateInfo input_assembly;
};

class PipelineLayoutCfg
{
  public:
    void setPipelineLayout( vk::Device device )
    {
        pipeline_layout = device.createPipelineLayoutUnique( vk::PipelineLayoutCreateInfo{} );
    }

    // clang-format off
    void make( vk::GraphicsPipelineCreateInfo* create_info )
    {
	create_info->setLayout( pipeline_layout.get() );
    }
    // clang-format on

  private:
    vk::UniquePipelineLayout pipeline_layout;
};

class BlendStateCfg
{
  public:
    BlendStateCfg()
    {
        // clang-format off
        color_blend_attachment.setColorWriteMask(
            vk::ColorComponentFlagBits::eA |
	    vk::ColorComponentFlagBits::eR |
	    vk::ColorComponentFlagBits::eG |
            vk::ColorComponentFlagBits::eB );
        // clang-format on

        color_blend_attachment.setBlendEnable( VK_FALSE );
        color_blending.setLogicOpEnable( VK_FALSE );
        color_blending.setAttachmentCount( 1 );
        color_blending.setPAttachments( &color_blend_attachment );
    }

    // clang-format off
    void make( vk::GraphicsPipelineCreateInfo* create_info )
    {
	create_info->setPColorBlendState( &color_blending );
    }
    // clang-format on

  private:
    vk::PipelineColorBlendAttachmentState color_blend_attachment;
    vk::PipelineColorBlendStateCreateInfo color_blending;
};

}; // namespace cfgs

//
template <PipelineCfg HeadCfg, PipelineCfg... Cfgs> class PipelineBuilder : public HeadCfg, public Cfgs...
{
  public:
    void make() { callMake<HeadCfg, Cfgs...>(); }

    vk::GraphicsPipelineCreateInfo& getCreateInfo() & { return create_info; }

  private:
    // Call make function of each base class
    template <typename Head, typename... Tail> void callMake()
    {
        Head::make( &create_info );

        if constexpr ( sizeof...( Tail ) != 0 )
        {
            callMake<Tail...>();
        }
    }

  private:
    vk::GraphicsPipelineCreateInfo create_info;
};

// clang-format off
using Pipeline =
    PipelineBuilder<cfgs::ShaderCfg,
		    cfgs::MultisamplingCfg,
		    cfgs::ViewportScissorCfg,
		    cfgs::RasterizerCfg,
		    cfgs::InputAssemblyCfg,
		    cfgs::PipelineLayoutCfg,
		    cfgs::BlendStateCfg>;

// clang-format on

}; // namespace vkwrap
