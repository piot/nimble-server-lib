/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/nimble-server-lib
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/
#include <clog/clog.h>
#include <nimble-server/errors.h>
#include <nimble-server/participant.h>
#include <nimble-server/participant_connection.h>
#include <nimble-server/participant_connections.h>
#include <nimble-server/transport_connection.h>
#include <secure-random/secure_random.h>

void nimbleServerParticipantConnectionsInit(NimbleServerParticipantConnections* self, size_t maxCount,
                                            ImprintAllocator* connectionAllocator,
                                            size_t maxNumberOfParticipantsForConnection,
                                            size_t maxSingleParticipantOctetCount, Clog log)
{
    self->connectionCount = 0;
    self->connections = IMPRINT_ALLOC_TYPE_COUNT(connectionAllocator, NimbleServerParticipantConnection, maxCount);
    self->capacityCount = maxCount;
    self->allocator = connectionAllocator;
    self->maxParticipantCountForConnection = maxNumberOfParticipantsForConnection;
    self->maxSingleParticipantStepOctetCount = maxSingleParticipantOctetCount;
    self->log = log;

    tc_mem_clear_type_n(self->connections, self->capacityCount);

    for (size_t i = 0; i < self->capacityCount; ++i) {
        Clog subLog;
        subLog.constantPrefix = "NimbleServerParticipantConnection";
        subLog.config = log.config;
        nimbleServerParticipantConnectionInit(&self->connections[i], 0, connectionAllocator, 0xffffffff,
                                              maxNumberOfParticipantsForConnection, maxSingleParticipantOctetCount,
                                              subLog);
    }
}

void nimbleServerParticipantConnectionsReset(NimbleServerParticipantConnections* self)
{
    for (size_t i = 0; i < self->capacityCount; ++i) {
        nimbleServerParticipantConnectionReset(&self->connections[i]);
    }
}

/// Finds the participant connection using the internal index
/// @param self participant connections
/// @param connectionIndex connection index to find
/// @return return NULL if connection was not found
struct NimbleServerParticipantConnection*
nimbleServerParticipantConnectionsFindConnection(NimbleServerParticipantConnections* self, uint8_t connectionIndex)
{
    if (connectionIndex >= self->capacityCount) {
        CLOG_C_ERROR(&self->log, "Illegal connection index: %d", connectionIndex)
        // return 0;
    }

    return &self->connections[connectionIndex];
}

/// Finds the participant connection for the specified connection id
/// @param self participant connections
/// @param transportConnectionId find participant connection for the specified transport connection
/// @return pointer to a participant connection, or NULL otherwise
struct NimbleServerParticipantConnection*
nimbleServerParticipantConnectionsFindConnectionForTransport(NimbleServerParticipantConnections* self,
                                                             uint32_t transportConnectionId)
{
    for (size_t i = 0; i < self->connectionCount; ++i) {
        if (self->connections[i].isUsed && self->connections[i].transportConnection &&
            self->connections[i].transportConnection->transportConnectionId == transportConnectionId) {
            return &self->connections[i];
        }
    }

    return 0;
}

/// Finds the participant connection with the specified secret
/// @param self participant connections
/// @param connectionSecret a previous secret given out by the server
/// @return pointer to a participant connection, or NULL otherwise
struct NimbleServerParticipantConnection*
nimbleServerParticipantConnectionsFindConnectionFromSecret(NimbleServerParticipantConnections* self,
                                                           NimbleSerializeParticipantConnectionSecret connectionSecret)
{
    for (size_t i = 0; i < self->connectionCount; ++i) {
        if (self->connections[i].isUsed && self->connections[i].secret == connectionSecret) {
            return &self->connections[i];
        }
    }

    return 0;
}

/// Finds the participant connection with the specified participantId (and that is waiting for reconnect)
/// @param self participant connections
/// @param participantId a participantId
/// @return pointer to a participant connection, or NULL otherwise
struct NimbleServerParticipantConnection*
nimbleServerParticipantConnectionsFindConnectionFromParticipantId(NimbleServerParticipantConnections* self,
                                                                  NimbleSerializeParticipantId participantId)
{
    CLOG_C_DEBUG(&self->log, "host migration: finding connection for participant id %hhu", participantId)
    for (size_t i = 0; i < self->connectionCount; ++i) {
        if (self->connections[i].isUsed &&
            self->connections[i].participantReferences.participantReferences[0]->id == participantId) {
            CLOG_C_DEBUG(&self->log, "host migration: found connection for participant id %hhu", participantId)
            if (self->connections[i].state == NimbleServerParticipantConnectionStateWaitingForReconnect) {
                return &self->connections[i];
            }
            CLOG_C_NOTICE(&self->log, "host migration: found the connection, but it was in the wrong state %d",
                          self->connections[i].state)
        }
    }

    CLOG_C_NOTICE(&self->log, "host migration: could not find prepared connection for participant id %hhu",
                  participantId)
    return 0;
}

static NimbleServerParticipantConnection* findFreeConnectionButDoNotReserveYet(NimbleServerParticipantConnections* self)
{
    for (size_t i = 0; i < self->capacityCount; ++i) {
        NimbleServerParticipantConnection* participantConnection = &self->connections[i];
        if (participantConnection->isUsed) {
            continue;
        }
        participantConnection->id = (uint32_t) i;
        CLOG_C_DEBUG(&self->log,
                     "found free participant connection to use at #%zu. capacity before allocating: (%zu/%zu)", i,
                     self->connectionCount, self->capacityCount)

        return participantConnection;
    }

    CLOG_C_NOTICE(&self->log, "out of participant connections from the pre-allocated pool")

    return 0;
}

static void addParticipantConnection(NimbleServerParticipantConnections* self,
                                     NimbleServerParticipantConnection* participantConnection,
                                     struct NimbleServerTransportConnection* transportConnection,
                                     StepId latestAuthoritativeStepId, NimbleServerParticipants* gameParticipants,
                                     struct NimbleServerParticipant* createdParticipants[16],
                                     size_t localParticipantCount)
{
    tc_snprintf(participantConnection->debugPrefix, sizeof(participantConnection->debugPrefix), "%s/%u",
                self->log.constantPrefix, participantConnection->id);
    participantConnection->log.constantPrefix = participantConnection->debugPrefix;
    participantConnection->log.config = self->log.config;

    nimbleServerParticipantConnectionInit(participantConnection, transportConnection, self->allocator,
                                          latestAuthoritativeStepId, self->maxParticipantCountForConnection,
                                          self->maxSingleParticipantStepOctetCount, participantConnection->log);
    self->connectionCount++;
    participantConnection->isUsed = true;

    participantConnection->participantReferences.gameParticipants = gameParticipants;
    for (size_t participantIndex = 0; participantIndex < localParticipantCount; ++participantIndex) {
        participantConnection->participantReferences
            .participantReferences[participantIndex] = createdParticipants[participantIndex];
    }

    participantConnection->participantReferences.participantReferenceCount = localParticipantCount;

    participantConnection->secret = secureRandomUInt64();

    CLOG_C_DEBUG(&participantConnection->log, "participant connection is ready. All participants have joined")
}

int nimbleServerParticipantConnectionsPrepare(NimbleServerParticipantConnections* self,
                                              NimbleServerParticipants* gameParticipants,
                                              StepId latestAuthoritativeStepId,
                                              NimbleSerializeParticipantId participantId,
                                              NimbleServerParticipantConnection** outConnection)
{
    NimbleServerParticipantConnection* participantConnection = findFreeConnectionButDoNotReserveYet(self);
    if (!participantConnection) {
        CLOG_C_NOTICE(&self->log, "could not join, because out of participant connection memory")
        return NimbleServerErrOutOfParticipantMemory;
    }

    struct NimbleServerParticipant* createdParticipants[16];

    int errorCode = nimbleServerParticipantsPrepare(gameParticipants, participantId, createdParticipants);
    if (errorCode < 0) {
        *outConnection = 0;
        return errorCode;
    }

    addParticipantConnection(self, participantConnection, 0, latestAuthoritativeStepId, gameParticipants,
                             createdParticipants, 1);

    participantConnection->isUsed = true;
    participantConnection->state = NimbleServerParticipantConnectionStateWaitingForReconnect;
    participantConnection->waitingForReconnectTimer = 0;

    *outConnection = participantConnection;

    return 0;
}

/// Creates a participant connection for the specified room.
/// @note In this version there must be an existing game in the room.
/// @param self participant connections
/// @param gameParticipants participants collection
/// @param transportConnection the transport connection to create the participant connection for
/// @param joinInfo information about the joining participants
/// @param latestAuthoritativeStepId latest known authoritative stepID
/// @param localParticipantCount local participant count
/// @param[out] outConnection the created connection
/// @return negative on error
int nimbleServerParticipantConnectionsCreate(NimbleServerParticipantConnections* self,
                                             NimbleServerParticipants* gameParticipants,
                                             struct NimbleServerTransportConnection* transportConnection,
                                             const NimbleSerializeJoinGameRequestPlayer* joinInfo,
                                             StepId latestAuthoritativeStepId, size_t localParticipantCount,
                                             NimbleServerParticipantConnection** outConnection)
{
    NimbleServerParticipantConnection* participantConnection = findFreeConnectionButDoNotReserveYet(self);
    if (!participantConnection) {
        CLOG_C_NOTICE(&self->log, "could not join, because out of participant connection memory")
        return NimbleServerErrOutOfParticipantMemory;
    }

    struct NimbleServerParticipant* createdParticipants[16];

    int errorCode = nimbleServerParticipantsJoin(gameParticipants, joinInfo, localParticipantCount,
                                                 createdParticipants);
    if (errorCode < 0) {
        *outConnection = 0;
        return errorCode;
    }

    addParticipantConnection(self, participantConnection, transportConnection, latestAuthoritativeStepId,
                             gameParticipants, createdParticipants, localParticipantCount);

    *outConnection = participantConnection;

    return 0;
}

/*

CLOG_C_DEBUG(&self->log, "out of participant connections. %zu/%zu", self->connectionCount, self->capacityCount)

*outConnection = 0;

return -1;
}

*/

/// Remove participant connection from participant connections collection
/// @param self participant connections
/// @param connection connection to remove from collection
void nimbleServerParticipantConnectionsRemove(NimbleServerParticipantConnections* self,
                                              struct NimbleServerParticipantConnection* connection)
{
    (void) self;

    CLOG_ASSERT(connection->isUsed, "connection must be in use to be removed")

    CLOG_C_DEBUG(&self->log, "disconnecting connection %u ", connection->id)

    CLOG_ASSERT(self->connectionCount != 0, "Connection count is wrong %zu", self->connectionCount)

    self->connectionCount--;

    CLOG_C_NOTICE(&self->log, "now has %zu connections left", self->connectionCount)

    nimbleServerParticipantConnectionReset(connection);
}
