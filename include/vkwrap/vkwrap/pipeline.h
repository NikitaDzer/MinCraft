#pragma once

#include "common/vulkan_include.h"
#include "vkwrap/core.h"
#include "vkwrap/shader_module.h"
#include <cassert>
#include <range/v3/range.hpp>
#include <range/v3/view.hpp>
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
        m_shader_stages[ vertex_shader ].setStage( vk::ShaderStageFlagBits::eVertex );
        m_shader_stages[ vertex_shader ].setModule( shader_module );
        m_shader_stages[ vertex_shader ].setPName( "main" );
    }

    void setBindingDescriptions( auto&& binding_descr )
    {
        m_vertex_input_info.setVertexBindingDescriptionCount( static_cast<uint32_t>( binding_descr.size() ) );
        m_vertex_input_info.setPVertexBindingDescriptions( binding_descr.data() );
    };

    void setAttributeDescriptions( auto&& attribute_descr )
    {
        m_vertex_input_info.setVertexAttributeDescriptionCount( static_cast<uint32_t>( attribute_descr.size() ) );
        m_vertex_input_info.setPVertexAttributeDescriptions( attribute_descr.data() );
    }

    void setFragmentShader( vkwrap::ShaderModule& shader_module )
    {
        constexpr auto fragment_shader = 1;
        m_shader_stages[ fragment_shader ].setStage( vk::ShaderStageFlagBits::eFragment );
        m_shader_stages[ fragment_shader ].setModule( shader_module );
        m_shader_stages[ fragment_shader ].setPName( "main" );
    }

    void make( vk::GraphicsPipelineCreateInfo* create_info )
    {
        create_info->setStageCount( stages_count );
        create_info->setPStages( m_shader_stages );
        create_info->setPVertexInputState( &m_vertex_input_info );
    }

  private:
    constexpr static auto stages_count = 2;

  private:
    vk::PipelineShaderStageCreateInfo m_shader_stages[ stages_count ];
    vk::PipelineVertexInputStateCreateInfo m_vertex_input_info;
};

class MultisamplingCfg
{
  public:
    MultisamplingCfg()
    {
        m_multisampling.setRasterizationSamples( vk::SampleCountFlagBits::e1 );
        m_multisampling.setSampleShadingEnable( VK_FALSE );
        m_multisampling.setMinSampleShading( 1.0f );
        m_multisampling.setPSampleMask( nullptr );
        m_multisampling.setAlphaToCoverageEnable( VK_FALSE );
        m_multisampling.setAlphaToOneEnable( VK_FALSE );
    }

    // clang-format off
    void make( vk::GraphicsPipelineCreateInfo* create_info )
    {
	create_info->setPMultisampleState( &m_multisampling );
    }
    // clang-format on

  private:
    vk::PipelineMultisampleStateCreateInfo m_multisampling;
};

class ViewportScissorCfg
{
  public:
    ViewportScissorCfg()
    {
        m_viewport_state.setViewportCount( 1 );
        m_viewport_state.setScissorCount( 1 );

        m_dynamic_states[ 0 ] = vk::DynamicState::eViewport;
        m_dynamic_states[ 1 ] = vk::DynamicState::eScissor;

        m_dynamic_state_info.setDynamicStateCount( states_count );
        m_dynamic_state_info.setDynamicStates( m_dynamic_states );
    }

    void make( vk::GraphicsPipelineCreateInfo* create_info )
    {
        create_info->setPViewportState( &m_viewport_state );
        create_info->setPDynamicState( &m_dynamic_state_info );
    }

  private:
    constexpr static auto states_count = 2;

  private:
    vk::DynamicState m_dynamic_states[ states_count ];
    vk::PipelineDynamicStateCreateInfo m_dynamic_state_info;
    vk::PipelineViewportStateCreateInfo m_viewport_state;
};

class RasterizerCfg
{
  public:
    RasterizerCfg()
    {
        m_rasterizer.setDepthClampEnable( VK_FALSE );
        m_rasterizer.setRasterizerDiscardEnable( VK_FALSE );
        m_rasterizer.setPolygonMode( vk::PolygonMode::eFill );
        m_rasterizer.setLineWidth( 1.0f );
        m_rasterizer.setCullMode( vk::CullModeFlagBits::eBack );
        m_rasterizer.setFrontFace( vk::FrontFace::eClockwise );
        m_rasterizer.setDepthBiasEnable( VK_FALSE );
        m_rasterizer.setDepthBiasConstantFactor( 0.0f );
        m_rasterizer.setDepthBiasClamp( 0.0f );
        m_rasterizer.setDepthBiasSlopeFactor( 0.0f );
    }

    // clang-format off
    void make( vk::GraphicsPipelineCreateInfo* create_info )
    {
	create_info->setPRasterizationState( &m_rasterizer );
    }
    // clang-format on

  private:
    vk::PipelineRasterizationStateCreateInfo m_rasterizer;
};

class InputAssemblyCfg
{
  public:
    InputAssemblyCfg()
    {
        m_input_assembly.setTopology( vk::PrimitiveTopology::eTriangleList );
        m_input_assembly.setPrimitiveRestartEnable( VK_FALSE );
    }

    // clang-format off
    void make( vk::GraphicsPipelineCreateInfo* create_info )
    {
	create_info->setPInputAssemblyState( &m_input_assembly );
    }
    // clang-format on

  private:
    vk::PipelineInputAssemblyStateCreateInfo m_input_assembly;
};

class PipelineLayoutCfg
{
  public:
    template <typename Layout = ranges::empty_view<vk::DescriptorSetLayout>>
    void setPipelineLayout( vk::Device device, Layout&& layouts = {} )
    {
        vk::PipelineLayoutCreateInfo create_info{
            .setLayoutCount = static_cast<uint32_t>( layouts.size() ),
            .pSetLayouts = layouts.data() };

        m_pipeline_layout = device.createPipelineLayoutUnique( create_info );
    }

    // clang-format off
    void make( vk::GraphicsPipelineCreateInfo* create_info )
    {
	create_info->setLayout( m_pipeline_layout.get() );
    }
    // clang-format on

  private:
    vk::UniquePipelineLayout m_pipeline_layout;
};

class BlendStateCfg
{
  public:
    BlendStateCfg()
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
    }

    // clang-format off
    void make( vk::GraphicsPipelineCreateInfo* create_info )
    {
	create_info->setPColorBlendState( &m_color_blending );
    }
    // clang-format on

  private:
    vk::PipelineColorBlendAttachmentState m_color_blend_attachment;
    vk::PipelineColorBlendStateCreateInfo m_color_blending;
};

}; // namespace cfgs

//
template <PipelineCfg... Cfgs> class PipelineBuilder : public Cfgs...
{
  public:
    void make() { callMake<Cfgs...>(); }

    vk::GraphicsPipelineCreateInfo& getCreateInfo() & { return m_pipeline_create_info; }

  private:
    // Call make function of each base class
    template <typename Head = void, typename... Tail> void callMake()
    {
        // void used if no parameter pack was given
        if constexpr ( !std::is_void<Head>::value )
        {
            Head::make( &m_pipeline_create_info );
        }

        if constexpr ( sizeof...( Tail ) != 0 )
        {
            callMake<Tail...>();
        }
    }

  private:
    vk::GraphicsPipelineCreateInfo m_pipeline_create_info;
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
