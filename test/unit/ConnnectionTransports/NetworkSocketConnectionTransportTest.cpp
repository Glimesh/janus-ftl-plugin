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
        int sockets[2];
        if (socketpair(AF_LOCAL, SOCK_DGRAM, 0, sockets) == -1)
        {
            FAIL(Util::ErrnoToString(errno));
        }

        auto result = NetworkSocketConnectionTransport::Nonblocking(
            NetworkSocketConnectionKind::Udp,
            sockets[0],
            std::nullopt);
        INFO("ErrorMessage: " << result.ErrorMessage);
        REQUIRE(result.IsError == false);
        transport = std::move(result.Value);
        mockSocketPairFd = sockets[1];
    }

    std::unique_ptr<NetworkSocketConnectionTransport> transport;
    int mockSocketPairFd; // Tests may interactive with this, the transport holds the other half of the pair
};

TEST_CASE( "UDP transport can receive packets", "[udp][transport]" )
{
    UdpTestContext c;
    std::vector<std::byte> buffer;

    SECTION( "when no packets are available to read, reading does not block" )
    {
        auto result = c.transport->Read(buffer, std::chrono::milliseconds(0));
        if (result.IsError)
        {
            FAIL("ErrorMessage: " << result.ErrorMessage);
        }
        REQUIRE( result.Value == 0 );
        REQUIRE( buffer.size() == 0 );
    }

    std::vector<std::byte> firstPacket = Util::StringToByteVector("First Packet");
    std::vector<std::byte> secondPacket = Util::StringToByteVector("Second Packet");

    INFO( "when making two packets available to read" )
    {
        REQUIRE(write(c.mockSocketPairFd, firstPacket.data(), firstPacket.size()) == firstPacket.size());
        REQUIRE(write(c.mockSocketPairFd, secondPacket.data(), secondPacket.size()) == secondPacket.size());
    }

    INFO( "then the first read gets the first packet" )
    {
        auto result = c.transport->Read(buffer, std::chrono::milliseconds(0));
        if (result.IsError)
        {
            FAIL("ErrorMessage: " << result.ErrorMessage);
        }
        CHECK_THAT( buffer, Catch::Equals( firstPacket ) );
        CHECK( result.Value == firstPacket.size() );
    }

    INFO( "then the second read gets the second packet" )
    {
        auto result = c.transport->Read(buffer, std::chrono::milliseconds(0));
        if (result.IsError)
        {
            FAIL("ErrorMessage: " << result.ErrorMessage);
        }
        CHECK( result.IsError == false);
        CHECK_THAT( buffer, Catch::Equals( secondPacket ) );
        CHECK( result.Value == secondPacket.size() );
    }

    INFO( "then the third read gets no packet" )
    {
        auto result = c.transport->Read(buffer, std::chrono::milliseconds(0));
        if (result.IsError)
        {
            FAIL("ErrorMessage: " << result.ErrorMessage);
        }
        CHECK( result.Value == 0 );
        CHECK( buffer.size() == 0 );
    }
}
