/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/nimble-server-lib
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/
#include <clog/clog.h>
#include <datagram-transport/transport.h>
#include <datagram-transport/types.h>
#include <flood/in_stream.h>
#include <flood/out_stream.h>
#include <nimble-serialize/commands.h>
#include <nimble-serialize/debug.h>
#include <nimble-server/errors.h>
#include <nimble-server/game.h>
#include <nimble-server/participant.h>
#include <nimble-server/local_party.h>
#include <nimble-server/req_connect.h>
#include <nimble-server/req_download_game_state.h>
#include <nimble-server/req_download_game_state_ack.h>
#include <nimble-server/req_join_game.h>
#include <nimble-server/req_step.h>

/// Clean up participant references
/// @param participantReferences the participant references that should be removed.
static void removeReferencesFromGameParticipants(NimbleServerParticipantReferences* participantReferences)
{
    NimbleServerParticipants* gameParticipants = participantReferences->gameParticipants;

    for (size_t i = 0; i < participantReferences->participantReferenceCount; ++i) {
        NimbleServerParticipant* participant = participantReferences->participantReferences[i];
        nimbleServerParticipantsDestroy(gameParticipants, participant->id);
        participant = 0;
    }
}

/// Destroys a local party
/// @param parties Pointer to an instance of Local Parties
/// @param party The party be set in disconnected mode and removed from Local Parties.
static void destroyParty(NimbleServerLocalParties* parties,
                                 NimbleServerLocalParty* party)
{
    nimbleServerLocalPartyDestroy(party);

    removeReferencesFromGameParticipants(&party->participantReferences);

    nimbleServerLocalPartiesRemove(parties, party);
}

static void disconnectTransportConnection(NimbleServer* self, NimbleServerTransportConnection* transportConnection)
{
    nimbleServerCircularBufferWrite(&self->freeTransportConnectionList, (uint8_t) transportConnection->id);
    transportConnectionDisconnect(transportConnection);
}

/// Iterates over all parties in the server, performs a tick update, and disconnects parties
/// that are recommended to be dissolved.
/// @param self Pointer to an instance of NimbleServer.
static void tickParties(NimbleServer* self)
{
    for (size_t i = 0; i < self->localParties.capacityCount; ++i) {
        NimbleServerLocalParty* party = &self->localParties.parties[i];
        if (!party->isUsed) {
            continue;
        }

        bool isWorking = nimbleServerLocalPartyTick(party);
        if (!isWorking) {
            destroyParty(&self->localParties, party);
            // It is not sure that a local party has a transport connection. The party could have been prepared by host migration.
            if (party->transportConnection != 0) {
                disconnectTransportConnection(self, party->transportConnection);
            }
        }
    }
}

/// Updates the server
/// Mostly for keeping track of stats and book-keeping.
/// @param self server
/// @param now current local server time
/// @return negative one error
int nimbleServerUpdate(NimbleServer* self, MonotonicTimeMs now)
{
    int qualityError = nimbleServerUpdateQualityTick(&self->updateQuality);
    if (qualityError < 0) {
        CLOG_C_SOFT_ERROR(&self->log, "quality error %d", qualityError)
        return qualityError;
    }

    tickParties(self);

    nimbleServerReadFromMultiTransport(self);

    statsIntPerSecondUpdate(&self->authoritativeStepsPerSecondStat, now);

    self->statsCounter++;
    if ((self->statsCounter % 3000) == 0) {
        statsIntPerSecondDebugOutput(&self->authoritativeStepsPerSecondStat, &self->log, "composedSteps", "steps/s");
    }

    return 0;
}

/// Determines whether a server error is considered an external error.
/// This function assesses if a given error code matches a predefined set of error conditions classified
/// as external errors. External errors are those that arise from circumstances beyond the direct control,
/// such as serialization issues, sessions being full, receipt of datagrams from disconnected connections, etc
/// @param err The error code to evaluate.
/// @return true if the error is classified as an external error; false otherwise.
bool nimbleServerIsErrorExternal(int err)
{
    return err == NimbleServerErrSerialize || err == NimbleServerErrSessionFull ||
           err == NimbleServerErrDatagramFromDisconnectedConnection || err == NimbleServerErrOutOfParticipantMemory;
}

/// Handle an incoming request from a client identified by the connectionIndex
/// It uses the NimbleServerResponse to send datagrams back to the client
/// @param self server
/// @param transportIndex transport connection index that we received datagram from
/// @param data datagram payload
/// @param len octet count of data
/// @param response info on how to make a response
/// @return negative on error
int nimbleServerFeed(NimbleServer* self, uint8_t transportIndex, const uint8_t* data, size_t len,
                     NimbleServerResponse* response)
{
    CLOG_C_VERBOSE(&self->log, "feed: octetCount: %zu", len)
    FldInStream inStream;
    fldInStreamInit(&inStream, data, len);
    inStream.readDebugInfo = true;

    uint8_t connectionIndex;
    fldInStreamReadUInt8(&inStream, &connectionIndex);

#define UDP_MAX_SIZE (1200)
    static uint8_t buf[UDP_MAX_SIZE];
    FldOutStream outStream;
    fldOutStreamInit(&outStream, buf, UDP_MAX_SIZE);
    outStream.writeDebugInfo = true; // transportConnection->useDebugStreams;

    if (connectionIndex == 0) {
        // Minimal header for oob
        fldOutStreamWriteUInt8(&outStream, 0); // Write zero connection id

        uint8_t cmd;
        fldInStreamReadUInt8(&inStream, &cmd);
        int result = 0;
        switch (cmd) {
            case NimbleSerializeCmdConnectRequest:
                result = nimbleServerReqConnect(self, transportIndex, &inStream, &outStream);
                break;
            default:
                CLOG_C_NOTICE(&self->log, "received unknown oob command %hhu", cmd)
                return NimbleServerErrSerialize;
        }

        if (outStream.pos > datagramTransportMaxSize) {
            CLOG_C_SOFT_ERROR(&self->log,
                              "trying to send datagram that has too many octets: %zu out of %zu. Discarding it",
                              outStream.pos, datagramTransportMaxSize)
            return NimbleServerErrSerialize;
        }

        if (result < 0) {
            return result;
        }

        return response->transportOut->send(response->transportOut->self, buf, outStream.pos);
    }

    if (connectionIndex >= NIMBLE_NIMBLE_SERVER_MAX_TRANSPORT_CONNECTIONS) {
        CLOG_C_SOFT_ERROR(&self->log, "illegal connection index : %u", connectionIndex)
        return NimbleServerErrSerialize;
    }

    NimbleServerTransportConnection* transportConnection = &self->transportConnections[connectionIndex];
    if (!transportConnection->isUsed) {
        CLOG_C_VERBOSE(&self->log, "received a connectionIndex from an unused connection. probably an old datagram?")
        return NimbleServerErrSerialize;
    }

    if (transportConnection->phase == NbTransportConnectionPhaseDisconnected) {
        CLOG_C_SOFT_ERROR(&self->log, "connection:%u is disconnected, so disregarding datagram", connectionIndex)
        return NimbleServerErrSerialize;
    }

    if (transportConnection->transportIndex != transportIndex) {
        CLOG_C_VERBOSE(&self->log, "we received a datagram from wrong transport index. Expected %hhu but received %hhu",
                      transportConnection->transportIndex, transportIndex)
        return NimbleServerErrSerialize;
    }

    int verifyHashStatus = connectionLayerIncomingVerify(&transportConnection->incomingConnection, &inStream);
    if (verifyHashStatus < 0) {
        CLOG_C_VERBOSE(&self->log, "we received a datagram with a wrong hash. could be due to it being and old datagram?")
        return NimbleServerErrSerialize;
    }

    int error = orderedDatagramInLogicReceive(&transportConnection->orderedDatagramInLogic, &inStream);
    if (error < 0) {
        CLOG_C_VERBOSE(&self->log, "we received an out of order datagram, discarding")
        return NimbleServerErrSerialize;
    }

    uint16_t clientTime;
    fldInStreamReadUInt16(&inStream, &clientTime);

    uint8_t cmd;
    fldInStreamReadUInt8(&inStream, &cmd);

    if (cmd == NimbleSerializeCmdDownloadGameStateStatus) {
        // Special case, game state ack can send multiple datagrams as reply
        return nimbleServerReqDownloadGameStateAck(&self->game, transportConnection, &inStream, response->transportOut,
                                                   clientTime);
    }

    FldOutStreamStoredPosition finalizeSeekPosition;
    int err = transportConnectionPrepareHeader(transportConnection, &outStream, clientTime, &finalizeSeekPosition);
    if (err < 0) {
        return err;
    }

    int result;
    switch (cmd) {
        case NimbleSerializeCmdGameStep:
            CLOG_C_VERBOSE(&self->log, "CmdGameStep")
            result = nimbleServerReqGameStep(&self->game, transportConnection, &self->authoritativeStepsPerSecondStat,
                                             &self->localParties, &inStream, &outStream);
            break;
        case NimbleSerializeCmdJoinGameRequest:
            CLOG_C_VERBOSE(&self->log, "JoinGame")
            result = nimbleServerReqGameJoin(self, transportConnection, &inStream, &outStream);
            break;
        case NimbleSerializeCmdDownloadGameStateRequest:
            CLOG_C_VERBOSE(&self->log, "StateRequest")
            result = nimbleServerReqDownloadGameState(self, transportConnection, &inStream, &outStream);
            break;
        default:
            CLOG_SOFT_ERROR("nimbleServerFeed: unknown command %02X", data[0])
            return 0;
    }

    if (result < 0) {
        if (!nimbleServerIsErrorExternal(result)) {
            CLOG_C_SOFT_ERROR(&self->log, "error %d encountered for cmd: %s", result, nimbleSerializeCmdToString(cmd))
            return result;
        }
        // CLOG_C_NOTICE(&self->log, "accepting error %d", result)
        return result;
    }

    if (inStream.pos != inStream.size) {
        CLOG_C_ERROR(&self->log, "not read everything from buffer %zu of %zu", inStream.pos, inStream.size)
    }

    int commitError = transportConnectionCommitHeader(transportConnection, &outStream, finalizeSeekPosition);
    if (commitError < 0) {
        return commitError;
    }

    if (outStream.pos <= 4) {
        CLOG_C_ERROR(&self->log, "no reply to send")
        // return 0;
    }

    orderedDatagramOutLogicCommit(&transportConnection->orderedDatagramOutLogic);

    if (outStream.pos > datagramTransportMaxSize) {
        CLOG_C_SOFT_ERROR(&self->log, "trying to send datagram that has too many octets: %zu out of %zu. Discarding it",
                          outStream.pos, datagramTransportMaxSize)
        return NimbleServerErrSerialize;
    }

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

    nimbleServerLocalPartiesInit(&self->localParties, setup.maxConnectionCount, setup.memory,
                                           setup.maxParticipantCountForEachConnection,
                                           setup.maxSingleParticipantStepOctetCount, setup.log);
    self->pageAllocator = setup.memory;
    self->blobAllocator = setup.blobAllocator;
    self->applicationVersion = setup.applicationVersion;
    self->callbackObject = setup.callbackObject;
    self->setup = setup;

    self->transportConnections[0].assignedParty = 0;
    self->transportConnections[0].transportConnectionId = (uint8_t) 0;
    self->transportConnections[0].isUsed = false;

    nimbleServerCircularBufferInit(&self->freeTransportConnectionList);
    for (size_t i = 1; i < NIMBLE_NIMBLE_SERVER_MAX_TRANSPORT_CONNECTIONS; ++i) {
        self->transportConnections[i].assignedParty = 0;
        self->transportConnections[i].transportConnectionId = (uint8_t) i;
        self->transportConnections[i].isUsed = false;
        nimbleServerCircularBufferWrite(&self->freeTransportConnectionList, (uint8_t) i);
    }

    statsIntPerSecondInit(&self->authoritativeStepsPerSecondStat, setup.now, 1000);

    nimbleServerUpdateQualityInit(&self->updateQuality, self->setup.targetTickTimeMs);

    return 0;
}

static bool containsParticipantId(NimbleSerializeParticipantId participantIds[], size_t participantIdCount,
                                  NimbleSerializeParticipantId searchFor)
{
    for (size_t participantIndex = 0; participantIndex < participantIdCount; ++participantIndex) {
        if (searchFor == participantIds[participantIndex]) {
            return true;
        }
    }

    return false;
}

/// Prepares the server's participants and local parties for a host migration process.
///
/// This function clears all existing local parties and prepares each participant
/// connection based on the provided participant IDs. The connections are set in waitingForReconnect.
/// It is intended to support host migration scenarios where a client is setting up a new server.
///
/// @note Currently assumes only a single participant ID per connection and that the local
/// participant index is 0. Future versions should aim to remove these limitations
/// and support more complex scenarios.
///
/// @param self server
/// @param participantIds Array containing the unique identifiers for each participant
/// that are setup to be waiting for incoming connections
/// @param participantIdCount The number of elements in the @p participantIds array,
/// indicating the total number of participants involved in the migration.
///
/// @return negative on error
int nimbleServerHostMigration(NimbleServer* self, NimbleSerializeParticipantId participantIds[],
                              size_t participantIdCount)
{
    CLOG_C_INFO(&self->log, "prepare new host for migration")
    nimbleServerLocalPartiesReset(&self->localParties);

    for (size_t i = 0; i < participantIdCount; ++i) {
        NimbleServerLocalParty* outConnection;
        int err = nimbleServerLocalPartiesPrepare(&self->localParties, &self->game.participants,
                                                            self->game.authoritativeSteps.expectedWriteId - 1,
                                                            participantIds[i], &outConnection);
        if (err < 0) {
            return err;
        }
    }

    nimbleServerCircularBufferInit(&self->game.participants.freeList);
    for (NimbleSerializeParticipantId i = 0; i < self->game.participants.participantCapacity; ++i) {
        if (containsParticipantId(participantIds, participantIdCount, i)) {
            continue;
        }
        nimbleServerCircularBufferWrite(&self->game.participants.freeList, i);
    }

    return 0;
}

/// Reinitialize (reuse the memory) and set a new game state.
/// The gameState must be present for the first client that connects to the game.
/// @param self server
/// @return negative on error
int nimbleServerReInitWithGame(NimbleServer* self, StepId stepId, MonotonicTimeMs now)
{
    nimbleServerGameInit(&self->game, self->pageAllocator, self->setup.maxSingleParticipantStepOctetCount,
                         self->setup.maxParticipantCount, self->log);

    nbsStepsReInit(&self->game.authoritativeSteps, stepId);
    statsIntPerSecondInit(&self->authoritativeStepsPerSecondStat, now, 1000);
    nimbleServerLocalPartiesReset(&self->localParties);
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


    return 0;
}

/// Notify the server that a connection has been disconnected on the transport layer.
/// @param self server
/// @param connectionIndex transport connection index that disconnected
/// @return negative on error
int nimbleServerConnectionDisconnected(NimbleServer* self, uint8_t connectionIndex)
{
    NimbleServerLocalParty* foundConnection = nimbleServerLocalPartiesFindParty(
        &self->localParties, connectionIndex);
    if (!foundConnection) {
        return -2;
    }

    if (!foundConnection->isUsed) {
        return -4;
    }

    foundConnection->id = 0xff;
    foundConnection->isUsed = false;

    NimbleServerTransportConnection* transportConnection = &self->transportConnections[connectionIndex];
    transportConnection->orderedDatagramInLogic.hasReceivedInitialDatagram = false;

    return 0;
}

/// Resets the server
/// @param self server
void nimbleServerReset(NimbleServer* self)
{
    (void) self;
    // nimbleServerLocalPartiesReset(&self->localParties);
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
    uint8_t datagram[UDP_MAX_SIZE];
    // CLOG_C_VERBOSE(&self->log, "read all from transport")
    ReplyOnlyToConnection replyOnlyToConnection;
    replyOnlyToConnection.multiTransport = self->multiTransport;

    DatagramTransportOut responseTransport;

    const int maximumNumberOfDatagramsPerTick = 64;

    for (size_t i = 0; i < maximumNumberOfDatagramsPerTick; ++i) {
        ssize_t octetCountReceived = self->multiTransport.receiveFrom(self->multiTransport.self, &connectionId,
                                                                      datagram, UDP_MAX_SIZE);
        if (octetCountReceived == 0) {
            if (i > 10) {
                CLOG_C_NOTICE(&self->log, "high number of datagrams in one tick: %zu", i)
            }
            return 0;
        }

        if (octetCountReceived < 0) {
            return (int) octetCountReceived;
        }

        replyOnlyToConnection.connectionIndex = connectionId;
        responseTransport.self = &replyOnlyToConnection;
        responseTransport.send = sendOnlyToSpecifiedTransport;

        NimbleServerResponse response;
        response.transportOut = &responseTransport;

        int errorCode = nimbleServerFeed(self, (uint8_t) connectionId, datagram, (size_t) octetCountReceived,
                                         &response);
        if (errorCode < 0) {
            if (!nimbleServerIsErrorExternal(errorCode)) {
                CLOG_C_SOFT_ERROR(&self->log, "error on feed %d", errorCode)
            }
            return errorCode;
        }
    }

    return 0;
}
