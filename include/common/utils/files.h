#pragma once

#include "utils/concepts.h"

#include <range/v3/algorithm/copy.hpp>
#include <range/v3/range/conversion.hpp>
#include <range/v3/view/all.hpp>
#include <range/v3/view/transform.hpp>

#include <concepts>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

namespace utils
{

void
tryOpenFileStream( IsFStream auto& file, const std::filesystem::path& path, std::ios_base::openmode mode )
{
    auto old_flags = file.exceptions();
    file.exceptions( old_flags | std::ios::badbit | std::ios::failbit );
    file.open( path, mode );
    file.exceptions( old_flags );
}

inline std::string
readFile( const std::filesystem::path& input_path, std::ios::openmode mode = std::ios::in )
{
    std::ifstream ifs;
    tryOpenFileStream( ifs, input_path, mode );
    std::stringstream ss;
    ss << ifs.rdbuf();
    return ss.str();
}

inline std::vector<std::byte>
readFileRaw( const std::filesystem::path& input_path )
{
    auto string_contents = readFile( input_path, std::ios::binary );
    return ranges::views::transform( string_contents, []( char c ) { return static_cast<std::byte>( c ); } ) |
        ranges::to_vector;
}

} // namespace utils