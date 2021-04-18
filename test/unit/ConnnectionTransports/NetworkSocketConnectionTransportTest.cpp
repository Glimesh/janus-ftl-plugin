/**
 * @file NetworkSocketConnectionTransportTest.cpp
 * @author Daniel Stiner (danstiner@gmail.com)
 * @date 2021-04-17
 * @copyright Copyright (c) 2021 Daniel Stiner
 */

#include <fcntl.h>
#include <catch2/catch.hpp>
#include <optional>
#include <unistd.h>

#include "../../../src/ConnectionTransports/NetworkSocketConnectionTransport.h"
#include "../../../src/Utilities/Util.h"

struct UdpTestContext {
    UdpTestContext()
    {
        {
            int result = socketpair(AF_LOCAL, SOCK_DGRAM, 0, mockSocketFds);
            INFO(Util::ErrnoToString(errno));
            REQUIRE(result == 0);
        }

        {
            auto result = NetworkSocketConnectionTransport::Nonblocking(
                NetworkSocketConnectionKind::Udp,
                mockSocketFds[0],
                std::nullopt);
            INFO("ErrorMessage: " << result.ErrorMessage);
            REQUIRE(result.IsError == false);
            transport = std::move(result.Value);
        }
    }

    std::unique_ptr<NetworkSocketConnectionTransport> transport;
    int mockSocketFds[2]; // We mock the "socket" the transport interacts with by using pipes
};

SCENARIO( "Transport can receive UDP packets", "[udp][transport]" )
{
    GIVEN( "A UDP network transport" )
    {
        UdpTestContext c;
        std::vector<std::byte> buffer;

        WHEN( "no packets are available to read" )
        {
            THEN( "reading does not block" )
            {
                auto result = c.transport->Read(buffer, std::chrono::milliseconds(0));
                REQUIRE( result.IsError == false);
                REQUIRE( result.Value == 0 );
                REQUIRE( buffer.size() == 0 );
            }
        }

        WHEN( "packets are available to read" )
        {
            std::vector<std::byte> firstPacket = Util::StringToByteVector("First Packet");
            std::vector<std::byte> secondPacket = Util::StringToByteVector("Second Packet");

            REQUIRE(write(c.mockSocketFds[1], firstPacket.data(), firstPacket.size()) == firstPacket.size());
            REQUIRE(write(c.mockSocketFds[1], secondPacket.data(), secondPacket.size()) == secondPacket.size());

            THEN( "reading gets the first packet" )
            {
                auto result = c.transport->Read(buffer, std::chrono::milliseconds(0));
                INFO("ErrorMessage: " << result.ErrorMessage);
                REQUIRE( result.IsError == false);
                REQUIRE_THAT( buffer, Catch::Equals( firstPacket ) );
                REQUIRE( result.Value == firstPacket.size() );
            }

            
            THEN( "reading again gets the second packet" )
            {
                auto result = c.transport->Read(buffer, std::chrono::milliseconds(0));
                INFO("ErrorMessage: " << result.ErrorMessage);
                REQUIRE( result.IsError == false);
                REQUIRE_THAT( buffer, Catch::Equals( secondPacket ) );
                REQUIRE( result.Value == secondPacket.size() );
            }
        }
    }
}
