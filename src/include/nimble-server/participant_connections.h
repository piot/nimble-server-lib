/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#ifndef NIMBLE_SERVER_PARTICIPANT_CONNECTIONS_H
#define NIMBLE_SERVER_PARTICIPANT_CONNECTIONS_H

#include <stdint.h>
#include <stdlib.h>

#include <clog/clog.h>
#include <nimble-server/participants.h>
#include <nimble-steps/steps.h>

struct NimbleServerParticipantConnection;

struct ImprintAllocator;
struct ImprintAllocatorWithFree;
struct NimbleServerTransportConnection;

typedef struct NimbleServerParticipantConnections {
    struct NimbleServerParticipantConnection* connections;
    size_t connectionCount;
    size_t capacityCount;
    struct ImprintAllocator* allocator;
    struct ImprintAllocatorWithFree* allocatorWithFree;
    size_t maxParticipantCountForConnection;
    size_t maxSingleParticipantStepOctetCount;
    Clog log;
} NimbleServerParticipantConnections;

void nbdParticipantConnectionsInit(NimbleServerParticipantConnections* self, size_t maxCount,
                                   struct ImprintAllocator* connectionAllocator,
                                   struct ImprintAllocatorWithFree* blobAllocator,
                                   size_t maxNumberOfParticipantsForConnection, size_t maxSingleParticipantOctetCount,
                                   Clog log);
void nbdParticipantConnectionsReset(NimbleServerParticipantConnections* self);
void nbdParticipantConnectionsDestroy(NimbleServerParticipantConnections* self);
void nbdParticipantConnectionsRemove(NimbleServerParticipantConnections* self, struct NimbleServerParticipantConnection* connection);
struct NimbleServerParticipantConnection* nbdParticipantConnectionsFindConnection(NimbleServerParticipantConnections* self,
                                                                         uint8_t connectionIndex);
struct NimbleServerParticipantConnection* nbdParticipantConnectionsFindConnectionForTransport(NimbleServerParticipantConnections* self,
                                                                                     uint32_t transportConnectionId);
int nbdParticipantConnectionsCreate(NimbleServerParticipantConnections* self, NimbleServerParticipants* gameParticipants,
                                    struct NimbleServerTransportConnection* transportConnection, const NimbleServerParticipantJoinInfo* joinInfo,
                                    StepId latestAuthoritativeStepId, size_t localParticipantCount,
                                    struct NimbleServerParticipantConnection** outConnection);
#endif
