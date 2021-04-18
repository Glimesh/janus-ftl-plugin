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
                if (result.IsError)
                {
                    FAIL("ErrorMessage: " << result.ErrorMessage);
                }
                REQUIRE( result.Value == 0 );
                REQUIRE( buffer.size() == 0 );
            }
        }

        WHEN( "packets are available to read" )
        {
            std::vector<std::byte> firstPacket = Util::StringToByteVector("First Packet");
            std::vector<std::byte> secondPacket = Util::StringToByteVector("Second Packet");

            REQUIRE(write(c.mockSocketPairFd, firstPacket.data(), firstPacket.size()) == firstPacket.size());
            REQUIRE(write(c.mockSocketPairFd, secondPacket.data(), secondPacket.size()) == secondPacket.size());

            THEN( "first read gets the first packet" )
            {
                auto result = c.transport->Read(buffer, std::chrono::milliseconds(0));
                if (result.IsError)
                {
                    FAIL("ErrorMessage: " << result.ErrorMessage);
                }
                CHECK_THAT( buffer, Catch::Equals( firstPacket ) );
                CHECK( result.Value == firstPacket.size() );

                AND_THEN( "second read gets the second packet" )
                {
                    auto result = c.transport->Read(buffer, std::chrono::milliseconds(0));
                    if (result.IsError)
                    {
                        FAIL("ErrorMessage: " << result.ErrorMessage);
                    }
                    CHECK( result.IsError == false);
                    CHECK_THAT( buffer, Catch::Equals( secondPacket ) );
                    CHECK( result.Value == secondPacket.size() );

                    AND_THEN( "third read gets no packet" )
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
            }
        }
    }
}



#define CHAR_BUFSIZE 50


TEST_CASE("Sequence from zero is valid")
{
    int fd[2], len;
    char message[CHAR_BUFSIZE];
    
    if(socketpair(AF_LOCAL, SOCK_DGRAM, 0, fd) == -1) {
        FAIL();
    }
    
    /* If you write to fd[0], you read from fd[1], and vice versa. */
    
    /* Print a message into one end of the socket. */
    snprintf(message, CHAR_BUFSIZE, "A message written to fd[0]");
    write(fd[0], message, strlen(message) + 1);
    
    /* Print a message into the other end of the socket. */
    snprintf(message, CHAR_BUFSIZE, "A message written to fd[1]");
    write(fd[1], message, strlen(message) + 1);
    
    /* Read from the first socket the data written to the second. */
    len = recv(fd[0], message, CHAR_BUFSIZE-1, 0);
    message[len] = '\0';
    REQUIRE(len == 27);
    
    /* Read from the second socket the data written to the first. */
    len = recv(fd[1], message, CHAR_BUFSIZE-1, 0);
    message[len] = '\0';
    REQUIRE(len == 27);
    


    /* Print a message into one end of the socket. */
    snprintf(message, CHAR_BUFSIZE, "A second message written to fd[0]");
    write(fd[0], message, strlen(message) + 1);
    
    /* Print a message into the other end of the socket. */
    snprintf(message, CHAR_BUFSIZE, "A second message written to fd[1]");
    write(fd[1], message, strlen(message) + 1);
  
    /* Read from the first socket the data written to the second. */
    len = recv(fd[0], message, CHAR_BUFSIZE-1, 0);
    message[len] = '\0';
    REQUIRE(len == 34);
    
    /* Read from the second socket the data written to the first. */
    len = recv(fd[1], message, CHAR_BUFSIZE-1, 0);
    message[len] = '\0';
    REQUIRE(len == 34);
    
    close(fd[0]);
}