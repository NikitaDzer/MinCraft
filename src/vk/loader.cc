#include "common/vulkan_include.h"

#include "vkwrap/core.h"

VULKAN_HPP_DEFAULT_DISPATCH_LOADER_DYNAMIC_STORAGE;

namespace vkwrap
{

void
initializeLoader()
{
    static const vk::DynamicLoader s_dl;
    auto p_vkGetInstanceProcAddr = // Do not confuse with the C-API function vkGetInstanceProcAddr
        s_dl.getProcAddress<PFN_vkGetInstanceProcAddr>( "vkGetInstanceProcAddr" );
    VULKAN_HPP_DEFAULT_DISPATCHER.init( p_vkGetInstanceProcAddr );
} // initializeLoader

} // namespace vkwrap