/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <clog/clog.h>
#include <flood/out_stream.h>
#include <nimble-serialize/commands.h>
#include <nimble-serialize/debug.h>
#include <nimble-server/game.h>
#include <nimble-server/participant.h>
#include <nimble-server/participant_connection.h>
#include <nimble-server/req_download_game_state_ack.h>
#include <nimble-server/req_join_game.h>
#include <nimble-server/req_step.h>
#include <nimble-server/server.h>
#include <udp-transport/udp_transport.h>
#include <nimble-server/req_download_game_state.h>
#include <flood/in_stream.h>


int nbdServerFeed(NbdServer *self, uint8_t connectionIndex,
                  const uint8_t *data, size_t len, NbdResponse *response) {
    CLOG_C_VERBOSE(&self->clog, "nbdServerFeed: feed: %s octetCount: %zu",
                 nimbleSerializeCmdToString(data[2]), len)

    if (connectionIndex >= self->connections.capacityCount) {
        CLOG_SOFT_ERROR("illegal connection index : %u", connectionIndex)
    }
    NbdTransportConnection *transportConnection = 0;
    NbdParticipantConnection* connection = 0;

    FldInStream inStream;
    fldInStreamInit(&inStream, data, len);

    if (connectionIndex > 64) {
        CLOG_ERROR("can only support up to 64 connections")
        return -13;
    }

    transportConnection = &self->transportConnections[connectionIndex];

    orderedDatagramInLogicReceive(&transportConnection->orderedDatagramInLogic, &inStream);

    connection = transportConnection->assignedParticipantConnection;

    uint8_t cmd;

    fldInStreamReadUInt8(&inStream, &cmd);

    if (cmd == NimbleSerializeCmdDownloadGameStateStatus) {
        // Special case, game state ack can send multiple datagrams as reply
        return nbdReqDownloadGameStateAck(connection, transportConnection, &inStream, response->transportOut);
    }

#define UDP_MAX_SIZE (1200)
        static uint8_t buf[UDP_MAX_SIZE];
        FldOutStream outStream;
        fldOutStreamInit(&outStream, buf, UDP_MAX_SIZE);

        orderedDatagramOutLogicPrepare(&transportConnection->orderedDatagramOutLogic, &outStream);

    int result = -1;
    switch (cmd) {
        case NimbleSerializeCmdGameStep:
            result = nbdReqGameStep(&self->game, connection, &self->connections, &inStream, &outStream);
            break;
        case NimbleSerializeCmdJoinGameRequest:
            result = nbdReqGameJoin(self, transportConnection, &inStream, &outStream);
            break;
        case NimbleSerializeCmdDownloadGameStateRequest:
            result = nbdReqDownloadGameState(connection, self->pageAllocator, &self->game.latestState, &inStream, &outStream);
            break;
        default: CLOG_SOFT_ERROR("nbdServerFeed: unknown command %02X", data[0]);
            return 0;
    }

    if (result < 0) {
        CLOG_SOFT_ERROR("error %d encountered for cmd: %s", result, nimbleSerializeCmdToString(cmd))
        return result;
    }

    orderedDatagramOutLogicCommit(&transportConnection->orderedDatagramOutLogic);

    return response->transportOut->send(response->transportOut->self, buf,
                                        outStream.pos);
}

#define MAX_CLIENT_COUNT (32)
#define MAX_INPUT_OCTET_COUNT (1024)

/// Initialize nimble server
/// @param self
/// @param applicationVersion the version for the application
/// @param memory
/// @param blobAllocator
/// @return
int nbdServerInit(NbdServer *self, NimbleSerializeVersion applicationVersion, struct ImprintAllocator *memory, struct ImprintAllocatorWithFree *blobAllocator) {
    nbdParticipantConnectionsInit(&self->connections, MAX_CLIENT_COUNT, memory, blobAllocator, MAX_INPUT_OCTET_COUNT);
    self->pageAllocator = memory;
    self->blobAllocator = blobAllocator;
    self->applicationVersion = applicationVersion;
    clogInitFromGlobal(&self->clog, "NimbleServer");
    for (size_t i=0; i<64; ++i) {
        self->transportConnections[i].assignedParticipantConnection = 0;
        self->transportConnections[i].transportConnectionId = i;
    }
    return 0;
}

/// Reinitialize (reuse the memory) and set a new game state.
/// @param self
/// @param gameState
/// @param gameStateOctetCount maximum of 64K supported
/// @return
int nbdServerReInitWithGame(NbdServer *self, const uint8_t *gameState, size_t gameStateOctetCount) {
    nbdGameInit(&self->game, self->pageAllocator, 64 * 1024);
    nbdGameSetGameState(&self->game, 0, gameState, gameStateOctetCount);
    nbdParticipantConnectionsReset(&self->connections);
    return 0;
}


/// Notify the server that a connection has been connected on the transport layer.
/// @param self
/// @param connectionIndex
/// @param participants
/// @param participantCount
/// @return
int nbdServerConnectionConnected(NbdServer *self, uint8_t connectionIndex) {
    NbdParticipantConnection *foundConnection = nbdParticipantConnectionsFindConnection(&self->connections,
                                                                                        connectionIndex);
    if (!foundConnection) {
        return -2;
    }

    if (foundConnection->isUsed) {
        return -4;
    }

    //nbdParticipantConnectionReInit(foundConnection, 0, participants, participantCount);
    foundConnection->isUsed = true;

    NbdTransportConnection* transportConnection = &self->transportConnections[connectionIndex];
    orderedDatagramOutLogicInit(&transportConnection->orderedDatagramOutLogic);
    orderedDatagramInLogicInit(&transportConnection->orderedDatagramInLogic);
    return 0;
}

/// Notify the server that a connection has been disconnected on the transport layer.
/// @param self
/// @param connectionIndex
/// @return
int nbdServerConnectionDisconnected(NbdServer *self, uint8_t connectionIndex) {
    NbdParticipantConnection *foundConnection = nbdParticipantConnectionsFindConnection(&self->connections,
                                                                                        connectionIndex);
    if (!foundConnection) {
        return -2;
    }

    if (!foundConnection->isUsed) {
        return -4;
    }


    foundConnection->id = 0x100;
    foundConnection->isUsed = false;

    NbdTransportConnection* transportConnection = &self->transportConnections[connectionIndex];
    transportConnection->orderedDatagramInLogic.hasReceivedInitialDatagram = false;

    return 0;
}

void nbdServerReset(NbdServer *self) {
    //nbdParticipantConnectionsReset(&self->connections);
}

/// Free all resources made by the server
/// @param self
void nbdServerDestroy(NbdServer *self) {
    nbdParticipantConnectionsDestroy(&self->connections);
}
