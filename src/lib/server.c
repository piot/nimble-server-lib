/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <clog/clog.h>
#include <datagram-transport/transport.h>
#include <flood/in_stream.h>
#include <flood/out_stream.h>
#include <nimble-serialize/commands.h>
#include <nimble-serialize/debug.h>
#include <nimble-server/errors.h>
#include <nimble-server/game.h>
#include <nimble-server/participant.h>
#include <nimble-server/participant_connection.h>
#include <nimble-server/req_connect.h>
#include <nimble-server/req_download_game_state.h>
#include <nimble-server/req_download_game_state_ack.h>
#include <nimble-server/req_join_game.h>
#include <nimble-server/req_step.h>
#include <nimble-server/server.h>

/// Updates the server
/// Mostly for keeping track of stats and book-keeping.
/// @param self server
/// @param now current local server time
/// @return negative one error
int nimbleServerUpdate(NimbleServer* self, MonotonicTimeMs now)
{
    int qualityError = nimbleServerUpdateQualityTick(&self->updateQuality);
    if (qualityError < 0) {
        return qualityError;
    }

    nimbleServerReadFromMultiTransport(self);

    statsIntPerSecondUpdate(&self->authoritativeStepsPerSecondStat, now);

    self->statsCounter++;
    if ((self->statsCounter % 3000) == 0) {
        statsIntPerSecondDebugOutput(&self->authoritativeStepsPerSecondStat, &self->log, "composedSteps", "steps/s");
    }

    return 0;
}

static bool isAcceptableError(int err)
{
    return err == NimbleServerErrSerialize || err == NimbleServerErrSessionFull ||
           err == NimbleServerErrDatagramFromDisconnectedConnection || err == NimbleServerErrOutOfParticipantMemory;
}

/// Handle an incoming request from a client identified by the connectionIndex
/// It uses the NimbleServerResponse to send datagrams back to the client
/// @param self server
/// @param connectionIndex transport connection index that we received datagram from
/// @param data datagram payload
/// @param len octet count of data
/// @param response info on how to make a response
/// @return negative on error
int nimbleServerFeed(NimbleServer* self, uint8_t connectionIndex, const uint8_t* data, size_t len,
                     NimbleServerResponse* response)
{
    // CLOG_C_VERBOSE(&self->log, "feed: '%s' octetCount: %zu", nimbleSerializeCmdToString(data[5]), len)

    if (connectionIndex >= NIMBLE_NIMBLE_SERVER_MAX_TRANSPORT_CONNECTIONS) {
        CLOG_SOFT_ERROR("illegal connection index : %u", connectionIndex)
        return -83;
    }

    FldInStream inStream;
    fldInStreamInit(&inStream, data, len);
    inStream.readDebugInfo = true;

    if (connectionIndex > 64) {
        CLOG_SOFT_ERROR("can only support up to 64 connections")
        return -13;
    }

    NimbleServerTransportConnection* transportConnection = &self->transportConnections[connectionIndex];
    if (transportConnection->phase == NbTransportConnectionPhaseDisconnected) {
        CLOG_SOFT_ERROR("was disconnected")
        return -54;
    }

    orderedDatagramInLogicReceive(&transportConnection->orderedDatagramInLogic, &inStream);
    uint16_t clientTime;
    fldInStreamReadUInt16(&inStream, &clientTime);

    uint8_t cmd;
    fldInStreamReadUInt8(&inStream, &cmd);

    if (cmd == NimbleSerializeCmdDownloadGameStateStatus) {
        // Special case, game state ack can send multiple datagrams as reply
        return nimbleServerReqDownloadGameStateAck(transportConnection, &inStream, response->transportOut, clientTime);
    }

#define UDP_MAX_SIZE (1200)
    static uint8_t buf[UDP_MAX_SIZE];
    FldOutStream outStream;
    fldOutStreamInit(&outStream, buf, UDP_MAX_SIZE);
    outStream.writeDebugInfo = true; // transportConnection->useDebugStreams;

    int err = transportConnectionPrepareHeader(transportConnection, &outStream, clientTime);
    if (err < 0) {
        return err;
    }

    int result;
    switch (cmd) {
        case NimbleSerializeCmdConnectRequest:
            result = nimbleServerReqConnect(self, transportConnection, &inStream, &outStream);
            break;
        case NimbleSerializeCmdGameStep:
            result = nimbleServerReqGameStep(&self->game, transportConnection, &self->authoritativeStepsPerSecondStat,
                                             &self->connections, &inStream, &outStream);
            break;
        case NimbleSerializeCmdJoinGameRequest:
            result = nimbleServerReqGameJoin(self, transportConnection, &inStream, &outStream);
            break;
        case NimbleSerializeCmdDownloadGameStateRequest:
            result = nimbleServerReqDownloadGameState(transportConnection, self->pageAllocator, &self->game, &inStream,
                                                      &outStream);
            break;
        default:
            CLOG_SOFT_ERROR("nimbleServerFeed: unknown command %02X", data[0])
            return 0;
    }

    if (result < 0) {
        if (!isAcceptableError(result)) {
            CLOG_C_SOFT_ERROR(&self->log, "error %d encountered for cmd: %s", result, nimbleSerializeCmdToString(cmd))
            return result;
        } else {
            CLOG_C_NOTICE(&self->log, "accepting error %d", result)
        }
    }

    if (inStream.pos != inStream.size) {
        CLOG_C_ERROR(&self->log, "not read everything from buffer %zu of %zu", inStream.pos, inStream.size)
    }

    if (outStream.pos <= 4) {
        CLOG_C_ERROR(&self->log, "no reply to send")
        // return 0;
    }

    orderedDatagramOutLogicCommit(&transportConnection->orderedDatagramOutLogic);

    return response->transportOut->send(response->transportOut->self, buf, outStream.pos);
}

/// Initialize nimble server
/// @param self server
/// @param setup the initial server values
/// @return negative on error
int nimbleServerInit(NimbleServer* self, NimbleServerSetup setup)
{
    self->log = setup.log;

    CLOG_ASSERT(setup.memory != 0, "must provide memory to server setup")
    CLOG_ASSERT(setup.blobAllocator != 0, "must provide blobAllocator to server setup")

    self->multiTransport = setup.multiTransport;
    if (setup.maxConnectionCount > NIMBLE_NIMBLE_SERVER_MAX_TRANSPORT_CONNECTIONS) {
        CLOG_C_ERROR(&self->log, "illegal number of connections. %zu but max %d is supported", setup.maxConnectionCount,
                     NIMBLE_NIMBLE_SERVER_MAX_TRANSPORT_CONNECTIONS)
        // return -1;
    }

    if (setup.maxSingleParticipantStepOctetCount > NimbleStepMaxSingleStepOctetCount) {
        CLOG_C_ERROR(&self->log, "nimbleServerInit. Single step octet count is not allowed %zu of %zu",
                     setup.maxSingleParticipantStepOctetCount, NimbleStepMaxSingleStepOctetCount)
        // return -1;
    }

    const size_t maximumNumberOfParticipantsAllowed = NIMBLE_NIMBLE_SERVER_MAX_TRANSPORT_CONNECTIONS;
    if (setup.maxParticipantCount > maximumNumberOfParticipantsAllowed) {
        CLOG_C_ERROR(&self->log, "nimbleServerInit. maximum number of participant count is too high: %zu of %zu",
                     setup.maxParticipantCount, maximumNumberOfParticipantsAllowed)
        // return -1;
    }

    nimbleServerParticipantConnectionsInit(&self->connections, setup.maxConnectionCount, setup.memory,
                                           setup.maxParticipantCountForEachConnection,
                                           setup.maxSingleParticipantStepOctetCount, setup.log);
    self->pageAllocator = setup.memory;
    self->blobAllocator = setup.blobAllocator;
    self->applicationVersion = setup.applicationVersion;
    setup.targetTickTimeMs = 16; // TODO: remove this later
    self->setup = setup;
    for (size_t i = 0; i < NIMBLE_NIMBLE_SERVER_MAX_TRANSPORT_CONNECTIONS; ++i) {
        self->transportConnections[i].assignedParticipantConnection = 0;
        self->transportConnections[i].transportConnectionId = (uint8_t) i;
        self->transportConnections[i].isUsed = false;
    }

    statsIntPerSecondInit(&self->authoritativeStepsPerSecondStat, setup.now, 1000);

    nimbleServerUpdateQualityInit(&self->updateQuality, self->setup.targetTickTimeMs);

    return 0;
}

/// Reinitialize (reuse the memory) and set a new game state.
/// The gameState must be present for the first client that connects to the game.
/// @param self server
/// @param gameState game state to reinitialize with
/// @param gameStateOctetCount maximum of 64K supported
/// @return negative on error
int nimbleServerReInitWithGame(NimbleServer* self, const uint8_t* gameState, size_t gameStateOctetCount, StepId stepId,
                               MonotonicTimeMs now)
{
    nimbleServerGameInit(&self->game, self->pageAllocator, self->setup.maxSingleParticipantStepOctetCount,
                         self->setup.maxGameStateOctetCount, self->setup.maxParticipantCount, self->log);
    nimbleServerGameSetGameState(&self->game, stepId, gameState, gameStateOctetCount, &self->log);

    nbsStepsReInit(&self->game.authoritativeSteps, stepId);
    statsIntPerSecondInit(&self->authoritativeStepsPerSecondStat, now, 1000);
    nimbleServerParticipantConnectionsReset(&self->connections);
    nimbleServerUpdateQualityReInit(&self->updateQuality);
    self->statsCounter = 0;
    return 0;
}

/// Notify the server that a connection has been connected on the transport layer.
/// @param self server
/// @param connectionIndex connectionIndex that connected
/// @return negative on error
int nimbleServerConnectionConnected(NimbleServer* self, uint8_t connectionIndex)
{
    NimbleServerTransportConnection* transportConnection = &self->transportConnections[connectionIndex];
    if (transportConnection->isUsed) {
        CLOG_C_SOFT_ERROR(&self->log, "connection %d already connected", connectionIndex)
        return -44;
    }

    CLOG_C_DEBUG(&self->log, "connection %d connected", connectionIndex)

    transportConnection->isUsed = true;
    transportConnectionInit(transportConnection, self->blobAllocator, self->setup.maxGameStateOctetCount, self->log);

    return 0;
}

/// Notify the server that a connection has been disconnected on the transport layer.
/// @param self server
/// @param connectionIndex transport connection index that disconnected
/// @return negative on error
int nimbleServerConnectionDisconnected(NimbleServer* self, uint8_t connectionIndex)
{
    NimbleServerParticipantConnection* foundConnection = nimbleServerParticipantConnectionsFindConnection(
        &self->connections, connectionIndex);
    if (!foundConnection) {
        return -2;
    }

    if (!foundConnection->isUsed) {
        return -4;
    }

    foundConnection->id = 0x100;
    foundConnection->isUsed = false;

    NimbleServerTransportConnection* transportConnection = &self->transportConnections[connectionIndex];
    transportConnection->orderedDatagramInLogic.hasReceivedInitialDatagram = false;

    return 0;
}

const static size_t NIMBLE_SERVER_REASONABLE_NUMBER_OF_STEPS_TO_CATCHUP_FOR_JOINERS = 80;

/// Indicates if a serialized game state must be provided this tick
/// @param self server
/// @return true if game state must be provided
bool nimbleServerMustProvideGameState(const NimbleServer* self)
{
    int deltaTickCountSinceLastGameState = (int) self->game.authoritativeSteps.expectedWriteId -
                                           (int) self->game.latestState.stepId;
    return deltaTickCountSinceLastGameState > (int) NIMBLE_SERVER_REASONABLE_NUMBER_OF_STEPS_TO_CATCHUP_FOR_JOINERS;
}

/// Sets the game state
/// Should only be called if nimbleServerMustProvideGameState() returns true.
/// @param self server
/// @param gameState game state to set
/// @param gameStateOctetCount number of octets in gameState
/// @param stepId stepID for the game state
void nimbleServerSetGameState(NimbleServer* self, const uint8_t* gameState, size_t gameStateOctetCount, StepId stepId)
{
    CLOG_C_DEBUG(&self->log, "game state was set locally for stepId %08X (%zu octetCount)", stepId, gameStateOctetCount)
    nimbleServerGameSetGameState(&self->game, stepId, gameState, gameStateOctetCount, &self->log);
}

/// Resets the server
/// @param self server
void nimbleServerReset(NimbleServer* self)
{
    (void) self;
    // nimbleServerParticipantConnectionsReset(&self->connections);
}

typedef struct ReplyOnlyToConnection {
    int connectionIndex;
    DatagramTransportMulti multiTransport;
} ReplyOnlyToConnection;

static int sendOnlyToSpecifiedTransport(void* _self, const uint8_t* data, size_t octetCount)
{
    ReplyOnlyToConnection* self = (ReplyOnlyToConnection*) _self;

    return self->multiTransport.sendTo(self->multiTransport.self, self->connectionIndex, data, octetCount);
}

/// Read all datagrams from the multi-transport
/// @param self server
int nimbleServerReadFromMultiTransport(NimbleServer* self)
{
    int connectionId;
    uint8_t datagram[1200];

    ReplyOnlyToConnection replyOnlyToConnection;
    replyOnlyToConnection.multiTransport = self->multiTransport;

    DatagramTransportOut responseTransport;

    const int maximumNumberOfConnections = 64;

    for (size_t i = 0; i < maximumNumberOfConnections; ++i) {
        ssize_t octetCountReceived = self->multiTransport.receiveFrom(self->multiTransport.self, &connectionId,
                                                                      datagram, 1200);
        if (octetCountReceived == 0) {
            if (i > 10) {
                CLOG_C_NOTICE(&self->log, "high number of datagrams in one tick: %zu", i)
            }
            return 0;
        }

        if (octetCountReceived < 0) {
            return (int) octetCountReceived;
        }
        bool didConnectNow = !self->transportConnections[connectionId].isUsed;

        if (didConnectNow) {
            nimbleServerConnectionConnected(self, (uint8_t) connectionId);
        }

        bool disconnectNow = self->transportConnections[connectionId].phase == NbTransportConnectionPhaseDisconnected;
        if (disconnectNow) {
            nimbleServerConnectionDisconnected(self, (uint8_t) connectionId);
        }

        replyOnlyToConnection.connectionIndex = connectionId;
        responseTransport.self = &replyOnlyToConnection;
        responseTransport.send = sendOnlyToSpecifiedTransport;

        NimbleServerResponse response;
        response.transportOut = &responseTransport;

        int errorCode = nimbleServerFeed(self, (uint8_t) connectionId, datagram, (size_t) octetCountReceived,
                                         &response);
        if (errorCode < 0) {
            CLOG_C_SOFT_ERROR(&self->log, "error on feed %d", errorCode)
            return errorCode;
        }
    }

    return 0;
}
