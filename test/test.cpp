/**
 * @file test.cpp
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2020-12-27
 * @copyright Copyright (c) 2020 Hayden McAfee
 */

// Some Catch2 defines required for PCH support
// https://github.com/catchorg/Catch2/blob/v2.x/docs/ci-and-misc.md#precompiled-headers-pchs
#undef TWOBLUECUBES_SINGLE_INCLUDE_CATCH_HPP_INCLUDED
#define CATCH_CONFIG_IMPL_ONLY
#define CATCH_CONFIG_RUNNER
#include <catch2/catch.hpp>

int main(int argc, char* argv[])
{
    // Test!
    int result = Catch::Session().run(argc, argv);
    return result;
}
