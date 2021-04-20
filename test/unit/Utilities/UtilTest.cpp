/**
 * @file UtilTest.cpp
 * @author Daniel Stiner (danstiner@gmail.com)
 * @date 2021-04-17
 * @copyright Copyright (c) 2021 Daniel Stiner
 */

#include <catch2/catch.hpp>

#include "../../../src/Utilities/Util.h"

TEST_CASE( "GenerateRandomBinaryPayload", "[utilities]" )
{
    auto firstPayload = Util::GenerateRandomBinaryPayload(10);
    REQUIRE(firstPayload.size() == 10);

    auto secondPayload = Util::GenerateRandomBinaryPayload(10);
    REQUIRE(secondPayload.size() == 10);
    
    REQUIRE_THAT( firstPayload, !Catch::Equals( secondPayload ) );
}
