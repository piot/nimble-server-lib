/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <clog/clog.h>
#include <flood/in_stream.h>
#include <flood/out_stream.h>
#include <nimble-serialize/commands.h>
#include <nimble-serialize/debug.h>
#include <nimble-server/game.h>
#include <nimble-server/participant.h>
#include <nimble-server/participant_connection.h>
#include <nimble-server/req_download_game_state.h>
#include <nimble-server/req_download_game_state_ack.h>
#include <nimble-server/req_join_game.h>
#include <nimble-server/req_step.h>
#include <nimble-server/server.h>
#include <udp-transport/udp_transport.h>

int nbdServerUpdate(NbdServer* self, MonotonicTimeMs now)
{
    statsIntPerSecondUpdate(&self->authoritativeStepsPerSecondStat, now);

    self->statsCounter++;
    if ((self->statsCounter % 3000) == 0) {
        statsIntPerSecondDebugOutput(&self->authoritativeStepsPerSecondStat, "composedSteps", "steps/s");
    }

    return 0;
}

int nbdServerFeed(NbdServer* self, uint8_t connectionIndex, const uint8_t* data, size_t len, NbdResponse* response)
{
    CLOG_C_VERBOSE(&self->log, "feed: '%s' octetCount: %zu", nimbleSerializeCmdToString(data[2]), len)

    if (connectionIndex >= self->connections.capacityCount) {
        CLOG_SOFT_ERROR("illegal connection index : %u", connectionIndex)
    }

    FldInStream inStream;
    fldInStreamInit(&inStream, data, len);

    if (connectionIndex > 64) {
        CLOG_ERROR("can only support up to 64 connections")
        return -13;
    }

    NbdTransportConnection* transportConnection = &self->transportConnections[connectionIndex];

    orderedDatagramInLogicReceive(&transportConnection->orderedDatagramInLogic, &inStream);

    uint8_t cmd;

    fldInStreamReadUInt8(&inStream, &cmd);

    if (cmd == NimbleSerializeCmdDownloadGameStateStatus) {
        // Special case, game state ack can send multiple datagrams as reply
        return nbdReqDownloadGameStateAck(transportConnection, &inStream, response->transportOut);
    }

#define UDP_MAX_SIZE (1200)
    static uint8_t buf[UDP_MAX_SIZE];
    FldOutStream outStream;
    fldOutStreamInit(&outStream, buf, UDP_MAX_SIZE);

    orderedDatagramOutLogicPrepare(&transportConnection->orderedDatagramOutLogic, &outStream);

    int result = -1;
    switch (cmd) {
        case NimbleSerializeCmdGameStep:
            result = nbdReqGameStep(&self->game, transportConnection, &self->authoritativeStepsPerSecondStat, &self->connections, &inStream, &outStream);
            break;
        case NimbleSerializeCmdJoinGameRequest:
            result = nbdReqGameJoin(self, transportConnection, &inStream, &outStream);
            break;
        case NimbleSerializeCmdDownloadGameStateRequest:
            result = nbdReqDownloadGameState(transportConnection, self->pageAllocator, &self->game,
                                             self->applicationVersion, &inStream, &outStream);
            break;
        default:
            CLOG_SOFT_ERROR("nbdServerFeed: unknown command %02X", data[0]);
            return 0;
    }

    if (result < 0) {
        CLOG_SOFT_ERROR("error %d encountered for cmd: %s", result, nimbleSerializeCmdToString(cmd))
        return result;
    }

    if (inStream.pos != inStream.size) {
        CLOG_ERROR("not read everything from buffer %d of %d", inStream.pos, inStream.size)
    }

    if (outStream.pos <= 2) {
        CLOG_C_ERROR(&self->log, "no reply to send")
        return 0;
    }

    orderedDatagramOutLogicCommit(&transportConnection->orderedDatagramOutLogic);

    return response->transportOut->send(response->transportOut->self, buf, outStream.pos);
}

/// Initialize nimble server
/// @param self
/// @param applicationVersion the version for the application
/// @param memory
/// @param blobAllocator
/// @return
int nbdServerInit(NbdServer* self, NbdServerSetup setup)
{
    self->log = setup.log;
    if (setup.maxConnectionCount > NBD_SERVER_MAX_TRANSPORT_CONNECTIONS) {
        CLOG_C_ERROR(&self->log, "illegal number of connections. %zu but max %d is supported", setup.maxConnectionCount,
                     NBD_SERVER_MAX_TRANSPORT_CONNECTIONS)
        return -1;
    }

    const size_t maximumSingleStepCountAllowed = 24;
    if (setup.maxSingleParticipantStepOctetCount > maximumSingleStepCountAllowed) {
        CLOG_C_ERROR(&self->log, "nbdServerInit. Single step octet count is not allowed %zu of %zu",
                     setup.maxSingleParticipantStepOctetCount, maximumSingleStepCountAllowed)
        return -1;
    }

    const size_t maximumNumberOfParticipantsAllowed = NBD_SERVER_MAX_TRANSPORT_CONNECTIONS;
    if (setup.maxParticipantCount > maximumNumberOfParticipantsAllowed) {
        CLOG_C_ERROR(&self->log, "nbdServerInit. maximum number of participant count is too high: %zu of %zu",
                     setup.maxParticipantCount, maximumNumberOfParticipantsAllowed)
        return -1;
    }

    nbdParticipantConnectionsInit(&self->connections, setup.maxConnectionCount, setup.memory, setup.blobAllocator,
                                  setup.maxParticipantCountForEachConnection, setup.maxSingleParticipantStepOctetCount,
                                  setup.log);
    self->pageAllocator = setup.memory;
    self->blobAllocator = setup.blobAllocator;
    self->applicationVersion = setup.applicationVersion;
    self->setup = setup;
    for (size_t i = 0; i < NBD_SERVER_MAX_TRANSPORT_CONNECTIONS; ++i) {
        self->transportConnections[i].assignedParticipantConnection = 0;
        self->transportConnections[i].transportConnectionId = i;
        self->transportConnections[i].isUsed = false;
    }

    statsIntPerSecondInit(&self->authoritativeStepsPerSecondStat, setup.now, 1000);

    return 0;
}

/// Reinitialize (reuse the memory) and set a new game state.
/// The gameState must be present for the first client that connects to the game.
/// @param self
/// @param gameState
/// @param gameStateOctetCount maximum of 64K supported
/// @return
int nbdServerReInitWithGame(NbdServer* self, const uint8_t* gameState, size_t gameStateOctetCount, StepId stepId, MonotonicTimeMs now)
{
    nbdGameInit(&self->game, self->pageAllocator, self->setup.maxSingleParticipantStepOctetCount,
                self->setup.maxGameStateOctetCount, self->setup.maxParticipantCount, self->log);
    nbdGameSetGameState(&self->game, stepId, gameState, gameStateOctetCount);

    nbsStepsReInit(&self->game.authoritativeSteps, stepId);
    statsIntPerSecondInit(&self->authoritativeStepsPerSecondStat, now, 1000);
    nbdParticipantConnectionsReset(&self->connections);
    self->statsCounter = 0;
    return 0;
}

/// Notify the server that a connection has been connected on the transport layer.
/// @param self
/// @param connectionIndex
/// @param participants
/// @param participantCount
/// @return
int nbdServerConnectionConnected(NbdServer* self, uint8_t connectionIndex)
{
    NbdTransportConnection* transportConnection = &self->transportConnections[connectionIndex];
    if (transportConnection->isUsed) {
        CLOG_C_ERROR(&self->log, "connection already connected")
        return -44;
    }

    /*
    NbdParticipantConnection* foundConnection = nbdParticipantConnectionsFindConnection(&self->connections,
                                                                                        connectionIndex);
                                                                                        */
    transportConnection->isUsed = true;
    transportConnectionInit(transportConnection, self->blobAllocator, self->log);

    return 0;
}

/// Notify the server that a connection has been disconnected on the transport layer.
/// @param self
/// @param connectionIndex
/// @return
int nbdServerConnectionDisconnected(NbdServer* self, uint8_t connectionIndex)
{
    NbdParticipantConnection* foundConnection = nbdParticipantConnectionsFindConnection(&self->connections,
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

const size_t NBD_REASONABLE_NUMBER_OF_STEPS_TO_CATCHUP_FOR_JOINERS = 80;

bool nbdServerMustProvideGameState(const NbdServer* self)
{
    int deltaTickCountSinceLastGameState = self->game.authoritativeSteps.expectedWriteId - self->game.latestState.stepId;
    return deltaTickCountSinceLastGameState > NBD_REASONABLE_NUMBER_OF_STEPS_TO_CATCHUP_FOR_JOINERS;
}

void nbdServerSetGameState(NbdServer* self, const uint8_t* gameState, size_t gameStateOctetCount, StepId stepId)
{
    CLOG_C_DEBUG(&self->log, "game state was set locally for stepId %08X (%zu octetCount)", stepId, gameStateOctetCount)
    nbdGameSetGameState(&self->game, stepId, gameState, gameStateOctetCount);
}

void nbdServerReset(NbdServer* self)
{
    // nbdParticipantConnectionsReset(&self->connections);
}

/// Free all resources made by the server
/// @param self
void nbdServerDestroy(NbdServer* self)
{
    nbdParticipantConnectionsDestroy(&self->connections);
}
