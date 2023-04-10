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

template <typename Callable> concept LambdaCfg = requires ( Callable c, vk::GraphicsPipelineCreateInfo& create_info )
{
    // operator() should take create_info by lvalue ref and only
    { c( create_info ) } -> std::same_as<void>;
    requires !requires { { c( vk::GraphicsPipelineCreateInfo{} ) } -> std::same_as<void>; };
};
// clang-format on

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

class Pipeline : private vk::UniquePipeline
{
  public:
    using BaseType = vk::UniquePipeline;
    using BaseType::get;

    Pipeline( vk::UniquePipeline unique_pipeline )
        : BaseType( std::move( unique_pipeline ) ){};

    operator vk::Pipeline() { return get(); }
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

    [[nodiscard]] Pipeline createPipeline( vk::Device device ) &
    {
        ( Cfgs<PipelineBuilder<Cfgs...>>::make( m_pipeline_create_info ), ... );

        return device.createGraphicsPipelineUnique( vk::PipelineCache{}, m_pipeline_create_info ).value;
    }

    vk::GraphicsPipelineCreateInfo& getCreateInfo() & { return m_pipeline_create_info; }

  private:
    vk::GraphicsPipelineCreateInfo m_pipeline_create_info;
};

using DefaultPipelineBuilder = PipelineBuilder<
    cfgs::ShaderCfg,
    cfgs::ViewportScissorCfg,
    cfgs::RasterizerCfg,
    cfgs::MultisamplingCfg,
    cfgs::InputAssemblyCfg,
    cfgs::PipelineLayoutCfg,
    cfgs::BlendStateCfg,
    cfgs::RenderPassCfg>;

}; // namespace vkwrap
