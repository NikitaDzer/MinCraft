#pragma once

#include "common/vulkan_include.h"

#include "utils/patchable.h"

#include "vkwrap/core.h"
#include "vkwrap/device.h"
#include "vkwrap/image_view.h"
#include "vkwrap/queues.h"
#include "vkwrap/surface.h"
#include "vkwrap/utils.h"

#include <range/v3/algorithm/sort.hpp>
#include <range/v3/range/concepts.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/range/traits.hpp>

#include <functional>
#include <optional>
#include <utility>
#include <variant>

#include <cassert>

namespace vkwrap
{

class SwapchainReqs
{

  public:
    struct WeightFormat
    {
        vk::SurfaceFormatKHR property;
        Weight weight;
    }; // WeightFormat

    struct WeightMode
    {
        vk::PresentModeKHR property;
        Weight weight;
    }; // WeightMode

    using WeightFormatVector = std::vector<WeightFormat>;
    using WeightModeVector = std::vector<WeightMode>;

  private:
    vk::SurfaceKHR m_surface;
    WeightFormatVector m_formats;
    WeightModeVector m_modes;
    uint32_t m_min_image_count;

    bool isDeviceSupportsSurface( vk::PhysicalDevice physical_device ) const
    {
        return physicalDeviceSupportsPresent( physical_device, m_surface );
    } // isDeviceSupportsSurface

    bool isSuitableMinImageCount( vk::PhysicalDevice physical_device ) const
    {
        auto available_image_count = physical_device.getSurfaceCapabilitiesKHR( m_surface ).minImageCount;
        return available_image_count >= m_min_image_count;
    } // isSuitableMinImageCount

    auto getAvailableFormats( vk::PhysicalDevice physical_device ) const
    {
        return physical_device.getSurfaceFormatsKHR( m_surface );
    } // getAvaiableFormats

    auto getAvailableModes( vk::PhysicalDevice physical_device ) const
    {
        return physical_device.getSurfacePresentModesKHR( m_surface );
    } // getAvailableModes

    Weight calculateWeightFormat( vk::PhysicalDevice physical_device ) const
    {
        auto available_formats = getAvailableFormats( physical_device );
        return calculateWeightProperty( available_formats, m_formats );
    } // calculateWeightFormat

    Weight calculateWeightMode( vk::PhysicalDevice physical_device ) const
    {
        auto available_modes = getAvailableModes( physical_device );
        return calculateWeightProperty( available_modes, m_modes );
    } // calculateWeightMode

    static Weight calculateWeightProperty(
        ranges::range auto&& available_properties,
        ranges::range auto&& req_weight_properties )
    {
        auto suitable_weight_property = findSuitableWeightProperty( available_properties, req_weight_properties );

        if ( suitable_weight_property.has_value() )
        {
            return suitable_weight_property->weight;
        }

        return k_bad_weight;
    } // calculateWeightProperty

    static auto findSuitableWeightProperty(
        ranges::range auto&& available_properties,
        ranges::range auto&& req_weight_properties )
        -> std::optional<ranges::range_value_t<decltype( req_weight_properties )>>
    {
        for ( auto&& req_weight_property : req_weight_properties )
        {
            if ( ranges::contains( available_properties, req_weight_property.property ) )
            {
                return std::optional{ req_weight_property };
            }
        }

        return std::nullopt;
    } // findSuitableWeightProperty

  public:
    SwapchainReqs(
        vk::SurfaceKHR surface,
        WeightFormatVector&& formats,
        WeightModeVector&& modes,
        uint32_t min_image_count )
        : m_surface{ surface },
          m_formats{ std::move( formats ) },
          m_modes{ std::move( modes ) },
          m_min_image_count{ min_image_count }
    {
    } // SwapchainReqs

    // clang-format off
    vk::SurfaceKHR            getSurface()       const  { return m_surface;         }
    const WeightFormatVector& getFormats()       const& { return m_formats;         }
    const WeightModeVector&   getModes()         const& { return m_modes;           }
    uint32_t                  getMinImageCount() const  { return m_min_image_count; }
    // clang-format on

    std::optional<vk::SurfaceFormatKHR> findSuitableFormat( vk::PhysicalDevice physical_device ) const
    {
        auto available_formats = getAvailableFormats( physical_device );
        auto weight_format = findSuitableWeightProperty( available_formats, m_formats );

        if ( weight_format.has_value() )
        {
            return std::optional{ weight_format->property };
        }

        return std::nullopt;
    } // findSuitableFormat

    std::optional<vk::PresentModeKHR> findSuitableMode( vk::PhysicalDevice physical_device ) const
    {
        auto available_modes = getAvailableModes( physical_device );
        auto weight_mode = findSuitableWeightProperty( available_modes, m_modes );

        if ( weight_mode.has_value() )
        {
            return std::optional{ weight_mode->property };
        }

        return std::nullopt;
    } // findSuitableMode

    Weight calculateWeight( vk::PhysicalDevice physical_device ) const
    {
        if ( !isDeviceSupportsSurface( physical_device ) )
        {
            return k_bad_weight;
        }

        if ( !isSuitableMinImageCount( physical_device ) )
        {
            return k_bad_weight;
        }

        Weight format_weight = calculateWeightFormat( physical_device );
        Weight mode_weight = calculateWeightMode( physical_device );

        return format_weight + mode_weight;
    } // calculateWeight

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
        ( std::optional<vk::SurfaceKHR>,     surface         ),
        ( std::optional<WeightFormatVector>, formats         ),
        ( std::optional<WeightModeVector>,   modes           ),
        ( std::optional<uint32_t>,           min_image_count ) 
    );
    // clang-format on

    using Setter = std::function<void( SwapchainReqsCreateInfo& )>;

  private:
    // clang-format off
    Setter m_setter = []( auto& ){};
    SwapchainReqsCreateInfo m_info;

    SwapchainReqsCreateInfo makeCreateInfo() const
    {
        SwapchainReqsCreateInfo info{};

        m_presetter( info );
        m_setter( info );
        info.patchWith( m_info );

        return info;
    } // makeCreateInfo

    static inline Setter m_presetter = []( auto& ){};
    // clang-format on    

    static void sortByAscendingWeight( ranges::range auto&& weight_properties )
    {
        ranges::sort( weight_properties, []( auto&& first, auto&& second ) { return first.weight < second.weight; } );
    } // sortByAscendingWeight

  public:
    SwapchainReqsBuilder() = default;

    SwapchainReqsBuilder& withSetter( Setter setter ) &
    {
        assert( setter );

        m_setter = setter;
        return *this;
    } // withSetter

    SwapchainReqsBuilder& withSurface( vk::SurfaceKHR surface ) &
    {
        m_info.surface = surface;
        return *this;
    } // withSurface

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
        SwapchainReqsCreateInfo create_info{ makeCreateInfo() };
        create_info.assertCheckMembers();

        sortByAscendingWeight( *create_info.formats );
        sortByAscendingWeight( *create_info.modes );

        return { *create_info.surface,
                 std::move( *create_info.formats ), 
                 std::move( *create_info.modes ), 
                 *create_info.min_image_count }; //
    } // make

    static void setPresetter( Setter presetter )
    {
        assert( presetter );
        m_presetter = presetter;
    } // setPresetter

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
    vk::SwapchainCreateInfoKHR m_create_info;
    vk::PhysicalDevice m_physical_device;
    vk::Device m_device;
    vk::SurfaceKHR m_surface;

    std::vector<uint32_t> m_indices;
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
        : m_create_info{ create_info },
          m_physical_device{ physical_device },
          m_device{ logical_device },
          m_surface{ surface }
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

    // clang-format off
    uint32_t     getImagesCount() const { return static_cast<uint32_t>( m_views.size() ); }
    vk::Format   getFormat()      const { return m_create_info.imageFormat;               }
    vk::Extent2D getExtent()      const { return m_create_info.imageExtent;               }
    // clang-format on

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

    static vk::SurfaceFormatKHR chooseSuitableFormat( const SwapchainReqs& reqs, vk::PhysicalDevice physical_device )
    {
        auto format = reqs.findSuitableFormat( physical_device );
        if ( format.has_value() )
        {
            return *format;
        }

        assert( 0 && "chooseSuitableFormat: Couldn't find any suitable format." );
        std::terminate();
    } // chooseSuitableFormat

    static vk::PresentModeKHR chooseSuitableMode( const SwapchainReqs& reqs, vk::PhysicalDevice physical_device )
    {
        auto mode = reqs.findSuitableMode( physical_device );
        if ( mode.has_value() )
        {
            return *mode;
        }

        assert( 0 && "chooseSuitableMode: Couldn't find any suitable present mode." );
        std::terminate();
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
        auto format = chooseSuitableFormat( reqs, physical_device );
        auto mode = chooseSuitableMode( reqs, physical_device );

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

    static void setPresetter( Setter presetter )
    {
        assert( presetter );
        m_presetter = presetter;
    } // setPresetter

}; // class SwapchainBuilder

} // namespace vkwrap
