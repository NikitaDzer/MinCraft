#pragma once

#include "common/vulkan_include.h"
#include "vkwrap/core.h"
#include <fstream>

namespace vkwrap
{
class ShaderModule : private vk::UniqueShaderModule
{
  public:
    using BaseType = vk::UniqueShaderModule;
    using BaseType::get;

    operator vk::ShaderModule() { return get(); }

    ShaderModule( const std::string& file_path, vk::Device device )
        : BaseType()
    {
        std::ifstream file{ file_path, std::ios::ate | std::ios::binary };

        if ( !file.is_open() )
        {
            throw Error{ "file " + file_path + " opening error" };
        }

        std::size_t file_size = file.tellg();
        std::vector<char> code_buffer( file_size );

        file.seekg( 0 );
        file.read( code_buffer.data(), file_size );
        file.close();

        vk::ShaderModuleCreateInfo create_info{
            .codeSize = code_buffer.size(),
            .pCode = reinterpret_cast<const uint32_t*>( code_buffer.data() ) };

        *static_cast<BaseType*>( this ) = device.createShaderModuleUnique( create_info );
    }
};
} // namespace vkwrap
