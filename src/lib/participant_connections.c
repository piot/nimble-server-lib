/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <clog/clog.h>
#include <nimble-server/participant_connection.h>
#include <nimble-server/participant_connections.h>

void nbdParticipantConnectionsInit(NbdParticipantConnections* self, size_t maxCount,
                                   ImprintAllocator* connectionAllocator, ImprintAllocatorWithFree* blobAllocator,
                                   size_t maxNumberOfParticipantsForConnection, size_t maxSingleParticipantOctetCount,
                                   Clog log)
{
    self->connectionCount = 0;
    self->connections = IMPRINT_ALLOC_TYPE_COUNT(connectionAllocator, NbdParticipantConnection, maxCount);
    self->capacityCount = maxCount;
    self->allocator = connectionAllocator;
    self->allocatorWithFree = blobAllocator;
    self->maxParticipantCountForConnection = maxNumberOfParticipantsForConnection;
    self->maxSingleParticipantStepOctetCount = maxSingleParticipantOctetCount;
    self->log = log;

    tc_mem_clear_type_n(self->connections, self->capacityCount);

    for (size_t i = 0; i < self->capacityCount; ++i) {
        Clog subLog;
        subLog.constantPrefix = "NbdParticipantConnection";
        subLog.config = log.config;
        nbdParticipantConnectionInit(&self->connections[i], 0, connectionAllocator, 0xffffffff,
                                     maxNumberOfParticipantsForConnection, maxSingleParticipantOctetCount, subLog);
    }
}

void nbdParticipantConnectionsDestroy(NbdParticipantConnections* self)
{
}

void nbdParticipantConnectionsReset(NbdParticipantConnections* self)
{
    for (size_t i = 0; i < self->capacityCount; ++i) {
        nbdParticipantConnectionReset(&self->connections[i]);
    }
}

/// Finds the participant connection using the internal index
/// @param self
/// @param connectionIndex
/// @return
struct NbdParticipantConnection* nbdParticipantConnectionsFindConnection(NbdParticipantConnections* self,
                                                                         uint8_t connectionIndex)
{
    if (connectionIndex >= self->capacityCount) {
        CLOG_C_ERROR(&self->log, "Illegal connection index: %d", connectionIndex)
        return 0;
    }

    return &self->connections[connectionIndex];
}

/// Finds the participant connection for the specified connection id
/// @param self
/// @param transportConnectionId
/// @return pointer to a participant connection, or 0 otherwise
struct NbdParticipantConnection* nbdParticipantConnectionsFindConnectionForTransport(NbdParticipantConnections* self,
                                                                                     uint32_t transportConnectionId)
{
    for (size_t i = 0; i < self->connectionCount; ++i) {
        if (self->connections[i].isUsed && self->connections[i].transportConnectionId == transportConnectionId) {
            return &self->connections[i];
        }
    }

    return 0;
}

/// Creates a participant connection for the specified room.
/// @note In this version there must be an existing game in the room.
/// @param self
/// @param transportConnectionId
/// @param ownerOfConnection
/// @param joinInfo
/// @param localParticipantCount
/// @param outConnection
/// @return error code
int nbdParticipantConnectionsCreate(NbdParticipantConnections* self, NbdParticipants* gameParticipants,
                                    size_t transportConnectionId, const NbdParticipantJoinInfo* joinInfo,
                                    StepId latestAuthoritativeStepId, size_t localParticipantCount,
                                    NbdParticipantConnection** outConnection)
{
    for (size_t i = 0; i < self->capacityCount; ++i) {
        NbdParticipantConnection* participantConnection = &self->connections[i];
        if (!participantConnection->isUsed) {
            struct NbdParticipant* createdParticipants[16];
            int errorCode = nbdParticipantsJoin(gameParticipants, joinInfo, localParticipantCount,
                                                createdParticipants);
            if (errorCode < 0) {
                *outConnection = 0;
                return errorCode;
            }

            participantConnection->id = i;
            nbdParticipantConnectionInit(participantConnection, transportConnectionId, self->allocator,
                                         latestAuthoritativeStepId, self->maxParticipantCountForConnection,
                                         self->maxSingleParticipantStepOctetCount, self->log);
            self->connectionCount++;
            participantConnection->isUsed = true;
            for (size_t participantIndex = 0; participantIndex < localParticipantCount; ++participantIndex) {
                participantConnection->participantReferences
                    .participantReferences[participantIndex] = createdParticipants[participantIndex];
            }
            participantConnection->participantReferences.participantReferenceCount = localParticipantCount;

            *outConnection = participantConnection;
            return 0;
        }
    }

    *outConnection = 0;

    return -1;
}
