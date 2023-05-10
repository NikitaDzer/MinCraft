#include "info_gui.h"

#include "vkwrap/core.h"
#include "vkwrap/device.h"
#include "vkwrap/instance.h"

#include <imgui.h>

#include <range/v3/algorithm/copy.hpp>
#include <range/v3/range/concepts.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/all.hpp>
#include <range/v3/view/transform.hpp>

#include <boost/preprocessor.hpp>
#include <spdlog/fmt/bundled/format.h>

namespace imgw
{

namespace
{

constexpr float k_indentation_width = 16.0f;

template <typename... Ts>
void
formatText( fmt::format_string<Ts...> formatstr, Ts&&... args )
{
    ImGui::Text( "%s", fmt::format( formatstr, std::forward<Ts>( args )... ).c_str() );
}

void
indent()
{
    ImGui::Indent( k_indentation_width );
}

void
unindent()
{
    ImGui::Unindent( k_indentation_width );
}

void
displayInstanceExtensions( ranges::range auto&& extensions )
{
    if ( ImGui::CollapsingHeader( "Supported extensions" ) )
    {
        indent();

        for ( auto&& extension : extensions )
        {
            formatText( "\"{}\" ver. {}", extension.extensionName.data(), extension.specVersion );
        }

        unindent();
    }
}

void
displayInstanceLayers( ranges::range auto&& layers )
{
    if ( ImGui::CollapsingHeader( "Supported layers" ) )
    {
        indent();

        for ( auto&& layer : layers )
        {
            formatText(
                "{}, s.ver. {}, ver. {}",
                layer.layerName.data(),
                vkwrap::versionToString( layer.specVersion ).c_str(),
                layer.implementationVersion );
        }

        unindent();
    }
}

void
displayInstanceInformation( const detail::InstanceInfo& info )
{
    if ( ImGui::CollapsingHeader( "Instance" ) )
    {
        indent();

        formatText( "Version: {}", vkwrap::versionToString( info.version() ) );
        displayInstanceExtensions( info.extensions() );
        displayInstanceLayers( info.layers() );

        unindent();
    }
}

template <typename T, auto N>
auto
tryConvertFormat( const vk::ArrayWrapper1D<T, N>& var )
{
    return fmt::format( "[{}]", fmt::join( var, ", " ) );
}

auto
tryConvertFormat( vk::Extent2D extent )
{
    return fmt::format( "[{}, {}]", extent.width, extent.height );
}

template <typename T>
auto
tryConvertFormat( T&& var )
    requires requires { fmt::format( "{}", var ); }
{
    if constexpr ( requires { vk::to_string( var ); } )
    {
        return vk::to_string( var );
    } else
    {
        return var;
    }
}

#define PRINT_LIMITS_MEMBER( object, member ) formatText( #member ": {}", tryConvertFormat( ( object.member ) ) )

void
displayPhysicalDeviceLimitsInformation( const vk::PhysicalDeviceLimits& limits )
{
    if ( ImGui::CollapsingHeader( "Limits" ) )
    {
        indent();

        PRINT_LIMITS_MEMBER( limits, maxImageDimension1D );
        PRINT_LIMITS_MEMBER( limits, maxImageDimension2D );
        PRINT_LIMITS_MEMBER( limits, maxImageDimension3D );
        PRINT_LIMITS_MEMBER( limits, maxImageDimensionCube );
        PRINT_LIMITS_MEMBER( limits, maxImageArrayLayers );
        PRINT_LIMITS_MEMBER( limits, maxTexelBufferElements );
        PRINT_LIMITS_MEMBER( limits, maxUniformBufferRange );
        PRINT_LIMITS_MEMBER( limits, maxStorageBufferRange );
        PRINT_LIMITS_MEMBER( limits, maxPushConstantsSize );
        PRINT_LIMITS_MEMBER( limits, maxMemoryAllocationCount );
        PRINT_LIMITS_MEMBER( limits, maxSamplerAllocationCount );
        PRINT_LIMITS_MEMBER( limits, bufferImageGranularity );
        PRINT_LIMITS_MEMBER( limits, sparseAddressSpaceSize );
        PRINT_LIMITS_MEMBER( limits, maxBoundDescriptorSets );
        PRINT_LIMITS_MEMBER( limits, maxPerStageDescriptorSamplers );
        PRINT_LIMITS_MEMBER( limits, maxPerStageDescriptorUniformBuffers );
        PRINT_LIMITS_MEMBER( limits, maxPerStageDescriptorStorageBuffers );
        PRINT_LIMITS_MEMBER( limits, maxPerStageDescriptorSampledImages );
        PRINT_LIMITS_MEMBER( limits, maxPerStageDescriptorStorageImages );
        PRINT_LIMITS_MEMBER( limits, maxPerStageDescriptorInputAttachments );
        PRINT_LIMITS_MEMBER( limits, maxDescriptorSetSamplers );
        PRINT_LIMITS_MEMBER( limits, maxDescriptorSetUniformBuffers );
        PRINT_LIMITS_MEMBER( limits, maxDescriptorSetUniformBuffersDynamic );
        PRINT_LIMITS_MEMBER( limits, maxDescriptorSetStorageBuffers );
        PRINT_LIMITS_MEMBER( limits, maxDescriptorSetStorageBuffersDynamic );
        PRINT_LIMITS_MEMBER( limits, maxDescriptorSetSampledImages );
        PRINT_LIMITS_MEMBER( limits, maxDescriptorSetStorageImages );
        PRINT_LIMITS_MEMBER( limits, maxDescriptorSetInputAttachments );
        PRINT_LIMITS_MEMBER( limits, maxVertexInputAttributes );
        PRINT_LIMITS_MEMBER( limits, maxVertexInputBindings );
        PRINT_LIMITS_MEMBER( limits, maxVertexInputAttributeOffset );
        PRINT_LIMITS_MEMBER( limits, maxVertexInputBindingStride );
        PRINT_LIMITS_MEMBER( limits, maxVertexOutputComponents );
        PRINT_LIMITS_MEMBER( limits, maxTessellationGenerationLevel );
        PRINT_LIMITS_MEMBER( limits, maxTessellationPatchSize );
        PRINT_LIMITS_MEMBER( limits, maxTessellationControlPerVertexInputComponents );
        PRINT_LIMITS_MEMBER( limits, maxTessellationControlPerVertexOutputComponents );
        PRINT_LIMITS_MEMBER( limits, maxTessellationControlPerPatchOutputComponents );
        PRINT_LIMITS_MEMBER( limits, maxTessellationControlTotalOutputComponents );
        PRINT_LIMITS_MEMBER( limits, maxTessellationEvaluationInputComponents );
        PRINT_LIMITS_MEMBER( limits, maxTessellationEvaluationOutputComponents );
        PRINT_LIMITS_MEMBER( limits, maxGeometryShaderInvocations );
        PRINT_LIMITS_MEMBER( limits, maxGeometryInputComponents );
        PRINT_LIMITS_MEMBER( limits, maxGeometryOutputComponents );
        PRINT_LIMITS_MEMBER( limits, maxGeometryOutputVertices );
        PRINT_LIMITS_MEMBER( limits, maxGeometryTotalOutputComponents );
        PRINT_LIMITS_MEMBER( limits, maxFragmentInputComponents );
        PRINT_LIMITS_MEMBER( limits, maxFragmentOutputAttachments );
        PRINT_LIMITS_MEMBER( limits, maxFragmentDualSrcAttachments );
        PRINT_LIMITS_MEMBER( limits, maxFragmentCombinedOutputResources );
        PRINT_LIMITS_MEMBER( limits, maxComputeSharedMemorySize );
        PRINT_LIMITS_MEMBER( limits, maxComputeWorkGroupCount );
        PRINT_LIMITS_MEMBER( limits, maxComputeWorkGroupInvocations );
        PRINT_LIMITS_MEMBER( limits, maxComputeWorkGroupSize );
        PRINT_LIMITS_MEMBER( limits, subPixelPrecisionBits );
        PRINT_LIMITS_MEMBER( limits, subTexelPrecisionBits );
        PRINT_LIMITS_MEMBER( limits, mipmapPrecisionBits );
        PRINT_LIMITS_MEMBER( limits, maxDrawIndexedIndexValue );
        PRINT_LIMITS_MEMBER( limits, maxDrawIndirectCount );
        PRINT_LIMITS_MEMBER( limits, maxSamplerLodBias );
        PRINT_LIMITS_MEMBER( limits, maxSamplerAnisotropy );
        PRINT_LIMITS_MEMBER( limits, maxViewports );
        PRINT_LIMITS_MEMBER( limits, maxViewportDimensions );
        PRINT_LIMITS_MEMBER( limits, viewportBoundsRange );
        PRINT_LIMITS_MEMBER( limits, viewportSubPixelBits );
        PRINT_LIMITS_MEMBER( limits, minMemoryMapAlignment );
        PRINT_LIMITS_MEMBER( limits, minTexelBufferOffsetAlignment );
        PRINT_LIMITS_MEMBER( limits, minUniformBufferOffsetAlignment );
        PRINT_LIMITS_MEMBER( limits, minStorageBufferOffsetAlignment );
        PRINT_LIMITS_MEMBER( limits, minTexelOffset );
        PRINT_LIMITS_MEMBER( limits, maxTexelOffset );
        PRINT_LIMITS_MEMBER( limits, minTexelGatherOffset );
        PRINT_LIMITS_MEMBER( limits, maxTexelGatherOffset );
        PRINT_LIMITS_MEMBER( limits, minInterpolationOffset );
        PRINT_LIMITS_MEMBER( limits, maxInterpolationOffset );
        PRINT_LIMITS_MEMBER( limits, subPixelInterpolationOffsetBits );
        PRINT_LIMITS_MEMBER( limits, maxFramebufferWidth );
        PRINT_LIMITS_MEMBER( limits, maxFramebufferHeight );
        PRINT_LIMITS_MEMBER( limits, maxFramebufferLayers );
        PRINT_LIMITS_MEMBER( limits, framebufferColorSampleCounts );
        PRINT_LIMITS_MEMBER( limits, framebufferDepthSampleCounts );
        PRINT_LIMITS_MEMBER( limits, framebufferStencilSampleCounts );
        PRINT_LIMITS_MEMBER( limits, framebufferNoAttachmentsSampleCounts );
        PRINT_LIMITS_MEMBER( limits, maxColorAttachments );
        PRINT_LIMITS_MEMBER( limits, sampledImageColorSampleCounts );
        PRINT_LIMITS_MEMBER( limits, sampledImageIntegerSampleCounts );
        PRINT_LIMITS_MEMBER( limits, sampledImageDepthSampleCounts );
        PRINT_LIMITS_MEMBER( limits, sampledImageStencilSampleCounts );
        PRINT_LIMITS_MEMBER( limits, storageImageSampleCounts );
        PRINT_LIMITS_MEMBER( limits, maxSampleMaskWords );
        PRINT_LIMITS_MEMBER( limits, timestampComputeAndGraphics );
        PRINT_LIMITS_MEMBER( limits, timestampPeriod );
        PRINT_LIMITS_MEMBER( limits, maxClipDistances );
        PRINT_LIMITS_MEMBER( limits, maxCullDistances );
        PRINT_LIMITS_MEMBER( limits, maxCombinedClipAndCullDistances );
        PRINT_LIMITS_MEMBER( limits, discreteQueuePriorities );
        PRINT_LIMITS_MEMBER( limits, pointSizeRange );
        PRINT_LIMITS_MEMBER( limits, lineWidthRange );
        PRINT_LIMITS_MEMBER( limits, pointSizeGranularity );
        PRINT_LIMITS_MEMBER( limits, lineWidthGranularity );
        PRINT_LIMITS_MEMBER( limits, strictLines );
        PRINT_LIMITS_MEMBER( limits, standardSampleLocations );
        PRINT_LIMITS_MEMBER( limits, optimalBufferCopyOffsetAlignment );
        PRINT_LIMITS_MEMBER( limits, optimalBufferCopyRowPitchAlignment );
        PRINT_LIMITS_MEMBER( limits, nonCoherentAtomSize );

        unindent();
    }
}

void
displayPhysicalDeviceExtensions( ranges::range auto&& extensions )
{
    if ( ImGui::CollapsingHeader( "Extensions" ) )
    {
        indent();

        for ( auto&& extension : extensions )
        {
            formatText( "\"{}\" ver. {}", extension.extensionName.data(), extension.specVersion );
        }

        unindent();
    }
}

#define PRINT_FEATURES_MEMBER( object, member ) formatText( #member ": {}", ( object.member ) )

void
displayPhysicalDeviceFeatures( const vk::PhysicalDeviceFeatures& features )
{
    if ( ImGui::CollapsingHeader( "Features" ) )
    {
        indent();

        PRINT_FEATURES_MEMBER( features, robustBufferAccess );
        PRINT_FEATURES_MEMBER( features, fullDrawIndexUint32 );
        PRINT_FEATURES_MEMBER( features, imageCubeArray );
        PRINT_FEATURES_MEMBER( features, independentBlend );
        PRINT_FEATURES_MEMBER( features, geometryShader );
        PRINT_FEATURES_MEMBER( features, tessellationShader );
        PRINT_FEATURES_MEMBER( features, sampleRateShading );
        PRINT_FEATURES_MEMBER( features, dualSrcBlend );
        PRINT_FEATURES_MEMBER( features, logicOp );
        PRINT_FEATURES_MEMBER( features, multiDrawIndirect );
        PRINT_FEATURES_MEMBER( features, drawIndirectFirstInstance );
        PRINT_FEATURES_MEMBER( features, depthClamp );
        PRINT_FEATURES_MEMBER( features, depthBiasClamp );
        PRINT_FEATURES_MEMBER( features, fillModeNonSolid );
        PRINT_FEATURES_MEMBER( features, depthBounds );
        PRINT_FEATURES_MEMBER( features, wideLines );
        PRINT_FEATURES_MEMBER( features, largePoints );
        PRINT_FEATURES_MEMBER( features, alphaToOne );
        PRINT_FEATURES_MEMBER( features, multiViewport );
        PRINT_FEATURES_MEMBER( features, samplerAnisotropy );
        PRINT_FEATURES_MEMBER( features, textureCompressionETC2 );
        PRINT_FEATURES_MEMBER( features, textureCompressionASTC_LDR );
        PRINT_FEATURES_MEMBER( features, textureCompressionBC );
        PRINT_FEATURES_MEMBER( features, occlusionQueryPrecise );
        PRINT_FEATURES_MEMBER( features, pipelineStatisticsQuery );
        PRINT_FEATURES_MEMBER( features, vertexPipelineStoresAndAtomics );
        PRINT_FEATURES_MEMBER( features, fragmentStoresAndAtomics );
        PRINT_FEATURES_MEMBER( features, shaderTessellationAndGeometryPointSize );
        PRINT_FEATURES_MEMBER( features, shaderImageGatherExtended );
        PRINT_FEATURES_MEMBER( features, shaderStorageImageExtendedFormats );
        PRINT_FEATURES_MEMBER( features, shaderStorageImageMultisample );
        PRINT_FEATURES_MEMBER( features, shaderStorageImageReadWithoutFormat );
        PRINT_FEATURES_MEMBER( features, shaderStorageImageWriteWithoutFormat );
        PRINT_FEATURES_MEMBER( features, shaderUniformBufferArrayDynamicIndexing );
        PRINT_FEATURES_MEMBER( features, shaderSampledImageArrayDynamicIndexing );
        PRINT_FEATURES_MEMBER( features, shaderStorageBufferArrayDynamicIndexing );
        PRINT_FEATURES_MEMBER( features, shaderStorageImageArrayDynamicIndexing );
        PRINT_FEATURES_MEMBER( features, shaderStorageImageWriteWithoutFormat );
        PRINT_FEATURES_MEMBER( features, shaderClipDistance );
        PRINT_FEATURES_MEMBER( features, shaderCullDistance );
        PRINT_FEATURES_MEMBER( features, shaderFloat64 );
        PRINT_FEATURES_MEMBER( features, shaderInt64 );
        PRINT_FEATURES_MEMBER( features, shaderInt16 );
        PRINT_FEATURES_MEMBER( features, shaderResourceResidency );
        PRINT_FEATURES_MEMBER( features, shaderResourceMinLod );
        PRINT_FEATURES_MEMBER( features, sparseBinding );
        PRINT_FEATURES_MEMBER( features, sparseResidencyBuffer );
        PRINT_FEATURES_MEMBER( features, sparseResidencyImage2D );
        PRINT_FEATURES_MEMBER( features, sparseResidencyImage3D );
        PRINT_FEATURES_MEMBER( features, sparseResidency2Samples );
        PRINT_FEATURES_MEMBER( features, sparseResidency4Samples );
        PRINT_FEATURES_MEMBER( features, sparseResidency8Samples );
        PRINT_FEATURES_MEMBER( features, sparseResidency16Samples );
        PRINT_FEATURES_MEMBER( features, sparseResidencyAliased );
        PRINT_FEATURES_MEMBER( features, variableMultisampleRate );
        PRINT_FEATURES_MEMBER( features, inheritedQueries );

        unindent();
    }
}

#define PRINT_COMMON_MEMBER( object, member ) formatText( #member ": {}", tryConvertFormat( object.member ) )

void
displaySurfaceInfo( const detail::SurfaceInfo& surface )
{
    if ( ImGui::CollapsingHeader( "Surface" ) )
    {
        indent();

        const auto& capabilities = surface.capabilities();

        PRINT_COMMON_MEMBER( capabilities, currentExtent );
        PRINT_COMMON_MEMBER( capabilities, currentTransform );
        PRINT_COMMON_MEMBER( capabilities, maxImageArrayLayers );
        PRINT_COMMON_MEMBER( capabilities, maxImageCount );
        PRINT_COMMON_MEMBER( capabilities, maxImageExtent );
        PRINT_COMMON_MEMBER( capabilities, minImageCount );
        PRINT_COMMON_MEMBER( capabilities, minImageExtent );
        PRINT_COMMON_MEMBER( capabilities, supportedCompositeAlpha );
        PRINT_COMMON_MEMBER( capabilities, supportedTransforms );
        PRINT_COMMON_MEMBER( capabilities, supportedUsageFlags );

        if ( ImGui::CollapsingHeader( "Modes" ) )
        {
            indent();

            for ( auto&& mode : surface.modes() )
            {
                formatText( "{}", vk::to_string( mode ) );
            }

            unindent();
        }

        if ( ImGui::CollapsingHeader( "Formats" ) )
        {
            indent();

            for ( auto&& format : surface.format() )
            {
                formatText( "[{}, {}]", vk::to_string( format.format ), vk::to_string( format.colorSpace ) );
            }

            unindent();
        }
    }

    unindent();
}

void
displayPhysicalDeviceInformation( const detail::PhysicalDeviceInfo& info )
{
    const auto& properties = info.properties();

    if ( ImGui::CollapsingHeader( properties.deviceName ) )
    {
        indent();

        formatText( "Name: {}", properties.deviceName );
        formatText( "Id: {:x}", properties.deviceID );
        formatText( "Type: {}", vk::to_string( properties.deviceType ) );
        formatText( "API ver.: {}", vkwrap::versionToString( properties.apiVersion ) );
        formatText( "Driver ver.: {:x}", properties.driverVersion );

        displayPhysicalDeviceLimitsInformation( properties.limits );
        displayPhysicalDeviceExtensions( info.extensions() );
        displayPhysicalDeviceFeatures( info.features() );
        displaySurfaceInfo( detail::SurfaceInfo{ info.device(), info.surface() } );

        unindent();
    }
}

void
displayPhysicalDevicesInformation( const detail::PhysicalDevicesInfo& info )
{
    if ( ImGui::CollapsingHeader( "Physical devices" ) )
    {
        indent();

        for ( auto&& device : info )
        {
            const auto& properties = device.properties();
            formatText( "{}: {}", vk::to_string( properties.deviceType ), properties.deviceName );
            displayPhysicalDeviceInformation( device );
        }

        unindent();
    }
}

} // namespace

std::vector<detail::PhysicalDeviceInfo>
detail::PhysicalDeviceInfo::allFromInstance( vk::Instance instance, vk::SurfaceKHR surface )
{
    auto devices = instance.enumeratePhysicalDevices();
    return ranges::views::transform(
               devices,
               [ surface ]( auto&& device ) {
                   return PhysicalDeviceInfo{ device, surface };
               } ) |
        ranges::to_vector;
}

void
VulkanInformationTab::draw() const
{
    ImGui::Begin( "Vulkan information" );

    displayInstanceInformation( m_information.instance );
    displayPhysicalDevicesInformation( m_information.physical_devices );

    ImGui::End();
}
}; // namespace imgw