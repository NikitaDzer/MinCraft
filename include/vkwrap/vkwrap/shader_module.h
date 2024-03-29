#pragma once

#include "common/vulkan_include.h"
#include "utils/files.h"
#include "vkwrap/core.h"

namespace vkwrap
{
class ShaderModule : private vk::UniqueShaderModule
{
  public:
    using BaseType = vk::UniqueShaderModule;
    using BaseType::get;

    operator vk::ShaderModule() const& { return get(); }

    ShaderModule( const std::filesystem::path& file_path, vk::Device device )
        : BaseType( create( file_path, device ) )
    {
    }

  private:
    BaseType create( const std::filesystem::path& file_path, vk::Device device )
    {
        auto code_buffer = utils::readFileRaw( file_path );

        vk::ShaderModuleCreateInfo create_info{
            .codeSize = code_buffer.size(),
            .pCode = reinterpret_cast<const uint32_t*>( code_buffer.data() ) };

        return device.createShaderModuleUnique( create_info );
    }
};
} // namespace vkwrap
