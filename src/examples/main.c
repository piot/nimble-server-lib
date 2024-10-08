/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/nimble-server-lib
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/

#include <clog/clog.h>
#include <clog/console.h>
#include <flood/in_stream.h>
#include <flood/out_stream.h>
#include <nimble-daemon/daemon.h>

#if !defined TORNADO_OS_WINDOWS
#include <unistd.h>
#endif

#include <imprint/default_setup.h>

#include <datagram-transport/transport.h>
#include <datagram-transport/types.h>
#include <nimble-daemon/version.h>

clog_config g_clog;

typedef struct UdpServerSocketSendToAddress {
    struct sockaddr_in* sockAddrIn;
    UdpServerSocket* serverSocket;
} UdpServerSocketSendToAddress;

static int sendToAddress(void* self_, const uint8_t* buf, size_t count)
{
    UdpServerSocketSendToAddress* self = (UdpServerSocketSendToAddress*) self_;

    return udpServerSend(self->serverSocket, buf, count, self->sockAddrIn);
}

int main(int argc, char* argv[])
{
    (void) argc;
    (void) argv;

    g_clog.log = clog_console;

    CLOG_OUTPUT("nimbled v%s starting up", NIMBLE_DAEMON_VERSION)

    NimbleServerDaemon daemon;

    int err = nimbleServerDaemonInit(&daemon);
    if (err < 0) {
        return err;
    }

    UdpServerSocketSendToAddress socketSendToAddress;
    socketSendToAddress.serverSocket = &daemon.socket;

    DatagramTransportOut transportOut;
    transportOut.self = &socketSendToAddress;
    transportOut.send = sendToAddress;

    NimbleServer server;

    ImprintDefaultSetup memory;
    imprintDefaultSetupInit(&memory, 16 * 1024 * 1024);

    NimbleSerializeVersion applicationVersion = {0x10, 0x20, 0x30};

    NimbleServerSetup setup;

    Clog serverLog;
    serverLog.constantPrefix = "example";
    serverLog.config = &g_clog;

    setup.applicationVersion = applicationVersion;
    setup.blobAllocator = &memory.slabAllocator.info;
    setup.log = serverLog;
    setup.maxConnectionCount = 8;
    setup.maxParticipantCount = 8;
    setup.maxParticipantCountForEachConnection = 2;
    setup.maxSingleParticipantStepOctetCount = 8;
    setup.memory = &memory.tagAllocator.info;

    nimbleServerInit(&server, setup);

    nimbleServerGameInit(&server.game, &memory.tagAllocator.info, setup.maxSingleParticipantStepOctetCount,
                         setup.maxGameStateOctetCount, setup.maxParticipantCount, setup.log);

    static uint8_t exampleGameState = 42;
    nimbleServerGameSetGameState(&server.game, 0, &exampleGameState, 1, &serverLog);


    uint8_t buf[DATAGRAM_TRANSPORT_MAX_SIZE];
    size_t size;
    struct sockaddr_in address;

    uint8_t reply[DATAGRAM_TRANSPORT_MAX_SIZE];
    FldOutStream outStream;
    fldOutStreamInit(&outStream, reply, DATAGRAM_TRANSPORT_MAX_SIZE);

    CLOG_OUTPUT("ready for incoming packets")

    while (1) {
        size = DATAGRAM_TRANSPORT_MAX_SIZE;
        ssize_t errorCode = udpServerReceive(&daemon.socket, buf, size, &address);
        if (errorCode < 0) {
            CLOG_WARN("problem with receive %zd", errorCode)
        } else {
            NimbleServerResponse response;
            response.transportOut = &transportOut;
            socketSendToAddress.sockAddrIn = &address;

            fldOutStreamRewind(&outStream);
#if CONFIGURATION_DEBUG
            nimbleSerializeDebugHex("received", buf, size);
#endif
            errorCode = nimbleServerFeed(&server, 1, buf, size, &response);
            if (errorCode < 0) {
                CLOG_WARN("nimbleServerFeed: error %zd", errorCode)
            }

            NbsSteps* authoritativeSteps = &server.game.authoritativeSteps;
            if (authoritativeSteps->stepsCount > 30) {
                nimbleServerGameSetGameState(&server.game, server.game.authoritativeSteps.expectedWriteId,
                                             &exampleGameState, 1, &serverLog);
            }
        }
    }

    // imprintDefaultSetupDestroy(&memory);

    // return 0;
}
