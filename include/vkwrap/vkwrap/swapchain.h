#pragma once

#include "common/vulkan_include.h"

#include "utils/patchable.h"

#include "vkwrap/core.h"
#include "vkwrap/utils.h"
#include "vkwrap/queues.h"
#include "vkwrap/surface.h"
#include "vkwrap/image_view.h"

#include <cstdint>
#include <iostream>
#include <optional>
#include <set>

#include "range/v3/algorithm/contains.hpp"
#include "range/v3/algorithm/sort.hpp"
#include <range/v3/iterator.hpp>
#include <range/v3/range/concepts.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view.hpp>
#include <range/v3/view/concat.hpp>

#include <type_traits>
#include <variant>

#include <algorithm>
#include <vulkan/vulkan_enums.hpp>

namespace vkwrap
{

class SwapchainReqs
{

  public:
    struct WeightFormat
    {
        vk::SurfaceFormatKHR format;
        uint32_t weight;
    }; // WeightFormat

    struct WeightMode
    {
        vk::PresentModeKHR mode;
        uint32_t weight;
    }; // WeightMode

    using WeightFormatVector = std::vector<WeightFormat>;
    using WeightModeVector = std::vector<WeightMode>;

  private:
    WeightFormatVector m_formats;
    WeightModeVector m_modes;
    uint32_t m_min_image_count;

  public:
    SwapchainReqs( WeightFormatVector&& formats, WeightModeVector&& modes, uint32_t min_image_count )
        : m_formats{ std::move( formats ) },
          m_modes{ std::move( modes ) },
          m_min_image_count{ min_image_count }
    {
    } // SwapchainReqs

    const WeightFormatVector& getFormats() const& { return m_formats; }
    const WeightModeVector& getModes() const& { return m_modes; }
    uint32_t getMinImageCount() const { return m_min_image_count; }

}; // class SwapchainReqs

class SwapchainReqsBuilder
{

  public:
    using WeightFormat = SwapchainReqs::WeightFormat;
    using WeightMode = SwapchainReqs::WeightMode;

    using WeightFormatVector = std::vector<WeightFormat>;
    using WeightModeVector = std::vector<WeightMode>;

    // clang-format off
    PATCHABLE_DEFINE_STRUCT(
        SwapchainReqsCreateInfo,
        ( std::optional<WeightFormatVector>, formats         ),
        ( std::optional<WeightModeVector>,   modes           ),
        ( std::optional<uint32_t>,           min_image_count ) 
    );
    // clang-format on

  private:
    SwapchainReqsCreateInfo m_info;

    static void sortByAscendingWeight( ranges::range auto& weight_properties )
    {
        ranges::sort( weight_properties, []( auto&& a, auto&& b ) { return a.weight > b.weight; } );
    } // sortByAscendingWeight

  public:
    SwapchainReqsBuilder() = default;

    SwapchainReqsBuilder& withFormats( ranges::range auto&& formats ) &
    {
        assert( !formats.empty() );

        m_info.formats = ranges::to_vector( formats );
        return *this;
    } // withFormats

    SwapchainReqsBuilder& withModes( ranges::range auto&& modes ) &
    {
        assert( !modes.empty() );

        m_info.modes = ranges::to_vector( modes );
        return *this;
    } // withModes

    SwapchainReqsBuilder& withMinImageCount( uint32_t min_image_count ) &
    {
        assert( min_image_count >= 1 );

        m_info.min_image_count = min_image_count;
        return *this;
    } // withMinImageCount

    SwapchainReqs make() const&
    {
        SwapchainReqsCreateInfo create_info{ m_info };
        create_info.assertCheckMembers();

        sortByAscendingWeight( *create_info.formats );
        sortByAscendingWeight( *create_info.modes );

        return { std::move( *create_info.formats ), 
                 std::move( *create_info.modes ), 
                 *create_info.min_image_count }; //
    } // make

}; // class SwapchainReqsBuilder

class Swapchain : private vk::UniqueSwapchainKHR
{

  private:
    using Base = vk::UniqueSwapchainKHR;

  public:
    struct AcquireResult
    {
        vk::Result result;
        uint32_t index;
    }; // struct AcquireResult

    enum class RecreateResult
    {
        k_success,
        k_window_minimized,
        k_need_specify_extent,
    }; // enum class RecreateResult

  private:
    vk::PhysicalDevice m_physical_device;
    vk::Device m_device;
    vk::SurfaceKHR m_surface;

    std::vector<uint32_t> m_indices;
    vk::SwapchainCreateInfoKHR m_create_info;
    std::vector<ImageView> m_views;

    void updateSwapchain()
    {
        vk::UniqueSwapchainKHR new_swapchain = m_device.createSwapchainKHRUnique( m_create_info );
        std::swap( static_cast<Base&>( *this ), new_swapchain );
    } // updateSwapchain

    void updateViews()
    {
        m_views.clear();

        auto images = m_device.getSwapchainImagesKHR( Base::get() );
        for ( auto&& image : images )
        {
            m_views.push_back( createView( image ) );
        }
    } // updateViews

    ImageView createView( vk::Image image ) const
    {
        ImageViewBuilder builder{};

        builder.withImage( image )
            .withImageType( k_image_type )
            .withFormat( m_create_info.imageFormat )
            .withComponents( k_components )
            .withLayerCount( m_create_info.imageArrayLayers );

        return builder.make( m_device );
    } // createView

    /**
     * Constants explanation:
     *
     * - k_image_type:  swapchain has images with only vk::ImageType::e2D
     * - k_components:  we don't need to remap components
     *
     * - k_zero_extent:    signals that window is minimized
     * - k_special_extent: signals that Swapchain user must choose extent
     *                     between surface minImageExtent and maxImageExent
     */
    static constexpr vk::ImageType k_image_type{ vk::ImageType::e2D };
    static constexpr vk::ComponentMapping k_components{};

    static constexpr vk::Extent2D k_zero_extent{ 0, 0 };
    static constexpr vk::Extent2D k_special_extent{ UINT32_MAX, UINT32_MAX };

  public:
    Swapchain(
        const vk::SwapchainCreateInfoKHR& create_info,
        vk::PhysicalDevice physical_device,
        vk::Device logical_device,
        vk::SurfaceKHR surface )
        : m_physical_device{ physical_device },
          m_device{ logical_device },
          m_surface{ surface },
          m_create_info{ create_info }
    {
        /**
         * There could be a dangling pointer,
         * so we need to save indices to `m_create_info` until the object destroyed.
         */
        m_indices = {
            create_info.pQueueFamilyIndices,
            create_info.pQueueFamilyIndices + create_info.queueFamilyIndexCount };

        m_create_info.pQueueFamilyIndices = m_indices.data();

        updateSwapchain();
        updateViews();
    }

    AcquireResult acquireNextImage(
        vk::Semaphore semaphore,
        vk::Fence fence = VK_NULL_HANDLE,
        uint64_t timeout = UINT64_MAX )
    {
        uint32_t index = 0;
        VkResult result = vkAcquireNextImageKHR( m_device, Base::get(), timeout, semaphore, fence, &index );

        vk::resultCheck(
            vk::Result{ result },
            "Swapchain: failed to acquire image",
            { vk::Result::eSuccess, vk::Result::eSuboptimalKHR, vk::Result::eErrorOutOfDateKHR } );

        return { vk::Result{ result }, index };
    } // acquireNextImage

    RecreateResult recreate( std::optional<vk::Extent2D> extent = {} )
    {
        if ( !extent.has_value() )
        {
            extent = getSurfaceExtent( m_physical_device, m_surface );
        }

        if ( extent == k_zero_extent )
        {
            return RecreateResult::k_window_minimized;
        }

        if ( extent == k_special_extent )
        {
            return RecreateResult::k_need_specify_extent;
        }

        m_create_info.setImageExtent( *extent );
        m_create_info.setOldSwapchain( Base::get() );

        updateSwapchain();
        updateViews();

        return RecreateResult::k_success;
    } // recreate

    RecreateResult recreateMinExtent()
    {
        auto min_extent = getSurfaceMinExtent( m_physical_device, m_surface );
        return recreate( min_extent );
    } // recreateMinExtent

    RecreateResult recreateMaxExtent()
    {
        auto max_extent = getSurfaceMaxExtent( m_physical_device, m_surface );
        return recreate( max_extent );
    } // recreateMaxExtent

    vk::ImageView getView( uint32_t index )
    {
        assert( index < m_views.size() );
        return m_views[ index ].get(); 
    } // getView

    uint32_t getImagesCount() const { return static_cast<uint32_t>( m_views.size() ); }

    vk::Format getFormat() const { return m_create_info.imageFormat; }
    vk::Extent2D getExtent() const { return m_create_info.imageExtent; }

    using Base::operator bool;
    using Base::operator->;
    using Base::get;

}; // class Swapchain

class SwapchainBuilder
{

  private:
    using Queues = std::vector<Queue>;

  public:
    enum class SurfaceTransformAdditionalFlags
    {
        k_current_transform
    }; // enum class SurfaceTransformAdditionalFlags

    using PreTransformFlag = std::variant<vk::SurfaceTransformFlagBitsKHR, SurfaceTransformAdditionalFlags>;

    // clang-format off
    PATCHABLE_DEFINE_STRUCT(
        SwapchainPartialInfo,
        ( std::optional<vk::SurfaceKHR>,                surface         ),
        ( std::optional<uint32_t>,                      min_image_count ),
        ( std::optional<vk::Extent2D>,                  image_extent    ),
        ( std::optional<vk::ImageUsageFlags>,           image_usage     ),
        ( std::optional<Queues>,                        queues          ),
        ( std::optional<PreTransformFlag>,              pre_transform   ),
        ( std::optional<vk::CompositeAlphaFlagBitsKHR>, composite_alpha ),
        ( std::optional<vk::Bool32>,                    clipped         ),
        ( std::optional<vk::SwapchainKHR>,              old_swapchain   )
    );
    // clang-format on

    using Setter = std::function<void( SwapchainPartialInfo& )>;

  private:
    // clang-format off
    Setter m_setter = []( auto& ){};
    // clang-format on

    SwapchainPartialInfo m_partial;

    SwapchainPartialInfo makePartialInfo() const
    {
        SwapchainPartialInfo partial{};

        m_presetter( partial );
        m_setter( partial );
        partial.patchWith( m_partial );

        return partial;
    } // makePartialInfo

    /**
     * This function expects req_properties to be sorted by ascending weight.
     */
    template <ranges::range Range>
    static auto chooseSuitableProperty( 
        const Range& available_properties, 
        const Range& req_properties ) //
    {
        for ( auto&& property : req_properties )
        {
            if ( ranges::contains( available_properties, property ) )
            {
                return property;
            }
        }

        assert( 0 && "chooseSuitableProperty: required properties are not available." );
        std::terminate();
    } // chooseSuitableProperty

    static vk::SurfaceFormatKHR chooseSuitableFormat(
        const SwapchainReqs& reqs,
        vk::PhysicalDevice physical_device,
        vk::SurfaceKHR surface )
    {
        auto available_formats = physical_device.getSurfaceFormatsKHR( surface );

        auto req_formats =
            ranges::views::transform( reqs.getFormats(), []( auto&& format ) { return format.format; } ) |
            ranges::to_vector;

        return chooseSuitableProperty( available_formats, req_formats );
    } // chooseSuitableFormat

    static vk::PresentModeKHR chooseSuitableMode(
        const SwapchainReqs& reqs,
        vk::PhysicalDevice physical_device,
        vk::SurfaceKHR surface )
    {
        auto available_modes = physical_device.getSurfacePresentModesKHR( surface );

        auto req_modes =
            ranges::views::transform( reqs.getModes(), []( auto&& mode ) { return mode.mode; } ) | 
            ranges::to_vector; //

        return chooseSuitableProperty( available_modes, req_modes );
    } // chooseSuitableMode

    static void writePreTransform(
        vk::SwapchainCreateInfoKHR& create_info,
        const PreTransformFlag& flag,
        vk::PhysicalDevice physical_device )
    {
        vk::SurfaceTransformFlagBitsKHR transform{};

        if ( const auto* vk_flag = std::get_if<vk::SurfaceTransformFlagBitsKHR>( &flag ) )
        {
            transform = *vk_flag;
        } else if ( std::get_if<SurfaceTransformAdditionalFlags>( &flag ) )
        {
            transform = getSurfaceCurrentTransform( physical_device, create_info.surface );
        } else
        {
            assert( 0 && "writePreTransform: Unimplemented variant type." );
            std::terminate();
        }

        create_info.preTransform = transform;
    } // writePreTransform

    // clang-format off
    static constexpr vk::SwapchainCreateInfoKHR k_initial_create_info = {
        // We don't use these specific fields.
        .pNext = {},
        .flags = {},

        // We don't need more than one layer in Swapchain images.
        .imageArrayLayers = 1,
    };

    static inline Setter m_presetter = []( auto& ){};
    // clang-format on

  public:
    SwapchainBuilder() = default;

    SwapchainBuilder& withSetter( Setter setter ) &
    {
        assert( setter );

        m_setter = setter;
        return *this;
    } // withSetter

    SwapchainBuilder& withSurface( vk::SurfaceKHR surface ) &
    {
        m_partial.surface = surface;
        return *this;
    } // withSurface

    SwapchainBuilder& withMinImageCount( uint32_t min_image_count ) &
    {
        assert( min_image_count >= 1 );

        m_partial.min_image_count = min_image_count;
        return *this;
    } // withMinImageCount

    SwapchainBuilder& withImageExtent( vk::Extent2D image_extent ) &
    {
        m_partial.image_extent = image_extent;
        return *this;
    } // withImageExtent

    SwapchainBuilder& withImageUsage( vk::ImageUsageFlags image_usage ) &
    {
        m_partial.image_usage = image_usage;
        return *this;
    } // withImageUsage

    SwapchainBuilder& withQueues( ranges::range auto&& queues ) &
    {
        assert( !queues.empty() );

        m_partial.queues = ranges::to_vector( queues );
        return *this;
    } // withQueues

    SwapchainBuilder& withPreTransform( PreTransformFlag pre_transform ) &
    {
        m_partial.pre_transform = pre_transform;
        return *this;
    } // withPreTransform

    SwapchainBuilder& withCompositeAlpha( vk::CompositeAlphaFlagBitsKHR composite_alpha ) &
    {
        m_partial.composite_alpha = composite_alpha;
        return *this;
    } // withCompositeAlpha

    SwapchainBuilder& withClipped( vk::Bool32 clipped ) &
    {
        m_partial.clipped = clipped;
        return *this;
    } // withClipped

    SwapchainBuilder& withOldSwapchain( vk::SwapchainKHR old_swapchain ) &
    {
        m_partial.old_swapchain = old_swapchain;
        return *this;
    } // withOldSwapchain

    Swapchain make( vk::Device device, vk::PhysicalDevice physical_device, const SwapchainReqs& reqs ) const&
    {
        SwapchainPartialInfo partial{ makePartialInfo() };
        partial.assertCheckMembers();

        auto surface = *m_partial.surface;
        auto format = chooseSuitableFormat( reqs, physical_device, surface );
        auto mode = chooseSuitableMode( reqs, physical_device, surface );

        vk::SwapchainCreateInfoKHR create_info{ k_initial_create_info };

        create_info.surface = surface;
        create_info.minImageCount = *partial.min_image_count;
        create_info.imageExtent = *partial.image_extent;
        create_info.imageUsage = *partial.image_usage;
        create_info.imageFormat = format.format;
        create_info.imageColorSpace = format.colorSpace;
        create_info.presentMode = mode;

        SharingInfoSetter setter{ *partial.queues };
        setter.setTo( create_info );

        writePreTransform( create_info, *partial.pre_transform, physical_device );

        create_info.compositeAlpha = *partial.composite_alpha;
        create_info.clipped = *partial.clipped;
        create_info.oldSwapchain = *partial.old_swapchain;

        return Swapchain{
            create_info,
            physical_device,
            device,
            surface,
        };
    } // make

    static void setPresetter( Setter presetter ) { m_presetter = presetter; }

}; // class SwapchainBuilder

} // namespace vkwrap
