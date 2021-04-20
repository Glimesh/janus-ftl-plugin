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

class UdpTestFixture {
public:
    UdpTestFixture()
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
        if (result.IsError)
        {
            FAIL("ErrorMessage: " << result.ErrorMessage);
        }
        transport = std::move(result.Value);
        mockSocketPairFd = sockets[1];
    }

    std::unique_ptr<NetworkSocketConnectionTransport> transport;
    int mockSocketPairFd; // One half of a socket pair, the transport holds the other half
};

TEST_CASE_METHOD(UdpTestFixture, "UDP transport can receive packets")
{
    std::vector<std::byte> buffer;

    SECTION( "when no packets are available to read, reading does not block" )
    {
        auto result = transport->Read(buffer, std::chrono::milliseconds(0));
        if (result.IsError)
        {
            FAIL("ErrorMessage: " << result.ErrorMessage);
        }
        REQUIRE( result.Value == 0 );
        REQUIRE( buffer.size() == 0 );
    }

    std::vector<std::byte> packet1 = Util::StringToByteVector("First Packet");
    std::vector<std::byte> packet2 = Util::StringToByteVector("Second Packet");

    INFO( "when making two packets available to read" )
    {
        REQUIRE(write(mockSocketPairFd, packet1.data(), packet1.size()) == (ssize_t)packet1.size());
        REQUIRE(write(mockSocketPairFd, packet2.data(), packet2.size()) == (ssize_t)packet2.size());
    }

    INFO( "then the first read gets the first packet" )
    {
        auto result = transport->Read(buffer, std::chrono::milliseconds(0));
        if (result.IsError)
        {
            FAIL("ErrorMessage: " << result.ErrorMessage);
        }
        CHECK_THAT( buffer, Catch::Equals( packet1 ) );
        CHECK( result.Value == (ssize_t)packet1.size() );
    }

    INFO( "then the second read gets the second packet" )
    {
        auto result = transport->Read(buffer, std::chrono::milliseconds(0));
        if (result.IsError)
        {
            FAIL("ErrorMessage: " << result.ErrorMessage);
        }
        CHECK( result.IsError == false);
        CHECK_THAT( buffer, Catch::Equals( packet2 ) );
        CHECK( result.Value == (ssize_t)packet2.size() );
    }

    INFO( "then the third read gets no packet" )
    {
        auto result = transport->Read(buffer, std::chrono::milliseconds(0));
        if (result.IsError)
        {
            FAIL("ErrorMessage: " << result.ErrorMessage);
        }
        CHECK( result.Value == 0 );
        CHECK( buffer.size() == 0 );
    }
}
