/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#ifndef NIMBLE_SERVER_PARTICIPANT_CONNECTIONS_H
#define NIMBLE_SERVER_PARTICIPANT_CONNECTIONS_H

#include <stdlib.h>
#include <stdint.h>

#include <nimble-server/participants.h>
#include <nimble-steps/steps.h>

struct NbdParticipantConnection;

struct ImprintAllocator;
struct ImprintAllocatorWithFree;

typedef struct NbdParticipantConnections {
    struct NbdParticipantConnection* connections;
    size_t connectionCount;
    size_t capacityCount;
    struct ImprintAllocator* allocator;
    struct ImprintAllocatorWithFree* allocatorWithFree;
} NbdParticipantConnections;

void nbdParticipantConnectionsInit(NbdParticipantConnections* self, size_t maxCount, struct ImprintAllocator* connectionAllocator, struct ImprintAllocatorWithFree* blobAllocator, size_t maxOctetCount);
void nbdParticipantConnectionsReset(NbdParticipantConnections* self);
void nbdParticipantConnectionsDestroy(NbdParticipantConnections* self);
struct NbdParticipantConnection* nbdParticipantConnectionsFindConnection(NbdParticipantConnections* self, uint8_t connectionIndex);
struct NbdParticipantConnection* nbdParticipantConnectionsFindConnectionForTransport(NbdParticipantConnections* self, uint32_t transportConnectionId);
int nbdParticipantConnectionsCreate(NbdParticipantConnections* self, NbdParticipants *gameParticipants,  size_t transportConnectionId, StepId currentStepId,
                                    const NbdParticipantJoinInfo* joinInfo, size_t localParticipantCount,
                                    struct NbdParticipantConnection** outConnection);
#endif
