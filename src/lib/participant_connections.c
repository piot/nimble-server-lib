/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <secure-random/secure_random.h>
#include <clog/clog.h>
#include <nimble-server/participant_connection.h>
#include <nimble-server/participant_connections.h>
#include <nimble-server/transport_connection.h>

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
        if (self->connections[i].isUsed &&
            self->connections[i].transportConnection->transportConnectionId == transportConnectionId) {
            return &self->connections[i];
        }
    }

    return 0;
}

/// Finds the participant connection with the specified secret (and that is waiting for reconnect)
/// @param self participant connections
/// @param connectionSecret a previous secret given out by the server
/// @return pointer to a participant connection, or NULL otherwise
struct NimbleServerParticipantConnection*
nimbleServerParticipantConnectionsFindConnectionFromSecret(NimbleServerParticipantConnections* self,
                                                           NimbleSerializeParticipantConnectionSecret connectionSecret)
{
    for (size_t i = 0; i < self->connectionCount; ++i) {
        if (self->connections[i].isUsed &&
            self->connections[i].state == NimbleServerParticipantConnectionStateWaitingForReconnect &&
            self->connections[i].secret == connectionSecret) {
            return &self->connections[i];
        }
    }

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
                                             const NimbleServerParticipantJoinInfo* joinInfo,
                                             StepId latestAuthoritativeStepId, size_t localParticipantCount,
                                             NimbleServerParticipantConnection** outConnection)
{
    for (size_t i = 0; i < self->capacityCount; ++i) {
        NimbleServerParticipantConnection* participantConnection = &self->connections[i];
        if (participantConnection->isUsed) {
            continue;
        }
        struct NimbleServerParticipant* createdParticipants[16];
        int errorCode = nimbleServerParticipantsJoin(gameParticipants, joinInfo, localParticipantCount,
                                                     createdParticipants);
        if (errorCode < 0) {
            *outConnection = 0;
            return errorCode;
        }

        participantConnection->id = (uint32_t) i;
        nimbleServerParticipantConnectionInit(participantConnection, transportConnection, self->allocator,
                                              latestAuthoritativeStepId, self->maxParticipantCountForConnection,
                                              self->maxSingleParticipantStepOctetCount, self->log);
        self->connectionCount++;
        participantConnection->isUsed = true;
        for (size_t participantIndex = 0; participantIndex < localParticipantCount; ++participantIndex) {
            participantConnection->participantReferences
                .participantReferences[participantIndex] = createdParticipants[participantIndex];
        }
        participantConnection->participantReferences.participantReferenceCount = localParticipantCount;

        participantConnection->secret = secureRandomUInt64();
        *outConnection = participantConnection;
        return 0;
    }

    *outConnection = 0;

    return -1;
}

/// Remove participant connection from participant connections collection
/// @param self participant connections
/// @param connection connection to remove from collection
void nimbleServerParticipantConnectionsRemove(NimbleServerParticipantConnections* self,
                                              struct NimbleServerParticipantConnection* connection)
{
    (void) self;
    CLOG_C_DEBUG(&self->log, "disconnecting connection %u forcedSteps:%zu impendingDisconnect:%zu", connection->id,
                 connection->forcedStepInRowCounter, connection->impedingDisconnectCounter)
    nimbleServerParticipantConnectionReset(connection);
}
