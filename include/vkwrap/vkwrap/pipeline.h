#pragma once

#include "vkwrap/pipeline_cfgs.h"
#include <type_traits>

namespace vkwrap
{

// clang-format off
template<typename T>
concept PipelineCfg = requires( T t, vk::GraphicsPipelineCreateInfo& create_info )
{
    // function make should take create_info by lvalue ref and only
    { t.make( create_info ) } -> std::same_as<void>;
    requires !requires { { t.make( vk::GraphicsPipelineCreateInfo{} ) } -> std::same_as<void>; };
};

// clang-format off
template <typename Callable> concept LambdaCfg = requires ( Callable c, vk::GraphicsPipelineCreateInfo& create_info )
{
    // operator() should take create_info by lvalue ref and only
    { c( create_info ) } -> std::same_as<void>;
    requires !requires { { c( vk::GraphicsPipelineCreateInfo{} ) } -> std::same_as<void>; };
}; // clang-format on

template <LambdaCfg... Lambdas> class LambdasWrapper : public Lambdas...
{
  public:
    LambdasWrapper( Lambdas&... lambdas )
        : Lambdas{ lambdas }...
    {
    }

    void make( vk::GraphicsPipelineCreateInfo& create_info )
    {
        // call () of all lambdas
        ( Lambdas::operator()( create_info ), ... );
    }
};

template <template <typename> typename... Cfgs> class PipelineBuilder : public Cfgs<PipelineBuilder<Cfgs...>>...
{
    // This static assert is used to check the constraints, becase PipelineBuilder can't be named in the
    // requires clause
    static_assert(
        ( PipelineCfg<Cfgs<PipelineBuilder>> && ... ),
        "Pipeline builder strategies do not satisfy all the necessary concepts" );

  public:
    template <LambdaCfg... Lambdas>
    PipelineBuilder( Lambdas&... lambdas )
        : Cfgs<PipelineBuilder<Cfgs...>>{}...
    {
        auto lambdas_wrapped = LambdasWrapper<Lambdas...>{ lambdas... };
        lambdas_wrapped.make( m_pipeline_create_info );
    }

    // clang-format off
    void createPipeline( vk::Device device ) &
    {
	( Cfgs<PipelineBuilder<Cfgs...>>::make( m_pipeline_create_info ), ... );

	auto create_res = device.createGraphicsPipelineUnique( vk::PipelineCache {}, m_pipeline_create_info );

        if ( create_res.result != vk::Result::eSuccess )
	{
	    throw Error{ "pipeline creation failed" };
	}

	m_pipeline = std::move( create_res.value );
    }
    // clang-format on

    vk::GraphicsPipelineCreateInfo& getCreateInfo() & { return m_pipeline_create_info; }

    vk::Pipeline& getPipeline() & { return m_pipeline.get(); }

  private:
    vk::GraphicsPipelineCreateInfo m_pipeline_create_info;
    vk::UniquePipeline m_pipeline;
};

// clang-format off
using DefaultPipelineBuilder = PipelineBuilder< cfgs::ShaderCfg,
                                                cfgs::ViewportScissorCfg,
                                                cfgs::RasterizerCfg,
						cfgs::MultisamplingCfg,
                                                cfgs::InputAssemblyCfg,
                                                cfgs::PipelineLayoutCfg,
                                                cfgs::BlendStateCfg,
                                                cfgs::RenderPassCfg>;
// clang-format on

}; // namespace vkwrap
