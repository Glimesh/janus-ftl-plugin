/**
 * @file test.cpp
 * @author Hayden McAfee (hayden@outlook.com)
 * @date 2020-12-27
 * @copyright Copyright (c) 2020 Hayden McAfee
 */

#define CATCH_CONFIG_RUNNER // This tells Catch that we'll be providing the main entrypoint
#include <catch2/catch.hpp>

int main(int argc, char* argv[])
{
    // Test!
    int result = Catch::Session().run(argc, argv);
    return result;
}