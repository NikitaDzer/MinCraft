#include <cassert>

#include "common/glfw_include.h"
#include "common/vulkan_include.h"

#include "vkwrap/core.h"
#include "vkwrap/surface.h"

#include <range/v3/algorithm/any_of.hpp>
#include <range/v3/view/iota.hpp>
#include <range/v3/view/take.hpp>
namespace vkwrap
{

bool
physicalDeviceSupportsPresent( vk::PhysicalDevice physical_device, vk::SurfaceKHR surface )
{
    assert( physical_device );

    QueueFamilyIndex queue_family_count = 0;
    physical_device.getQueueFamilyProperties(
        &queue_family_count,
        nullptr //
    );

    auto indices = ranges::views::iota( 0 ) | ranges::views::take( queue_family_count );
    return ranges::any_of( indices, [ physical_device, surface ]( auto&& index ) {
        return physical_device.getSurfaceSupportKHR( index, surface );
    } );
}

bool
Surface::isSupportedBy( vk::PhysicalDevice physical_device ) const
{
    return physicalDeviceSupportsPresent( physical_device, get() );
} // isSupportedBy

} // namespace vkwrap