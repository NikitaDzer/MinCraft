#pragma once

#include <range/v3/view/all.hpp>
#include <range/v3/view/filter.hpp>

#include <range/v3/algorithm/contains.hpp>

#include <range/v3/range/concepts.hpp>
#include <range/v3/range/conversion.hpp>

#include <vector>

namespace utils
{

// Algorithm to find all elements from `find` range of type FindRange that are not contained in `all`. Return a vector
// of type range_value_t<FindRange>;
template <ranges::range AllRange, ranges::range FindRange, typename Proj>
auto
findAllMissing( AllRange&& all, FindRange&& find, Proj&& proj )
{
    return ranges::views::all( std::forward<FindRange>( find ) ) |
        ranges::views::filter( [ &all, &proj ]( auto&& elem ) { return !ranges::contains( all, elem, proj ); } ) |
        ranges::to_vector;
}

}; // namespace utils