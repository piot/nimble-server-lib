/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/nimble-server-lib
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/
#ifndef NIMBLE_SERVER_PARTICIPANT_CONNECTIONS_H
#define NIMBLE_SERVER_PARTICIPANT_CONNECTIONS_H

#include <stdint.h>
#include <stdlib.h>

#include <clog/clog.h>
#include <nimble-server/participants.h>
#include <nimble-steps/steps.h>
#include <nimble-serialize/types.h>

struct NimbleServerParticipantConnection;

struct ImprintAllocator;
struct ImprintAllocatorWithFree;
struct NimbleServerTransportConnection;

typedef struct NimbleServerParticipantConnections {
    struct NimbleServerParticipantConnection* connections;
    size_t connectionCount;
    size_t capacityCount;
    struct ImprintAllocator* allocator;
    size_t maxParticipantCountForConnection;
    size_t maxSingleParticipantStepOctetCount;
    Clog log;
} NimbleServerParticipantConnections;

void nimbleServerParticipantConnectionsInit(NimbleServerParticipantConnections* self, size_t maxCount,
                                            struct ImprintAllocator* connectionAllocator,
                                            size_t maxNumberOfParticipantsForConnection,
                                            size_t maxSingleParticipantOctetCount, Clog log);
void nimbleServerParticipantConnectionsReset(NimbleServerParticipantConnections* self);
void nimbleServerParticipantConnectionsRemove(NimbleServerParticipantConnections* self,
                                              struct NimbleServerParticipantConnection* connection);
struct NimbleServerParticipantConnection*
nimbleServerParticipantConnectionsFindConnection(NimbleServerParticipantConnections* self, uint8_t connectionIndex);
struct NimbleServerParticipantConnection*
nimbleServerParticipantConnectionsFindConnectionForTransport(NimbleServerParticipantConnections* self,
                                                             uint32_t transportConnectionId);
struct NimbleServerParticipantConnection*
nimbleServerParticipantConnectionsFindConnectionFromSecret(NimbleServerParticipantConnections* self,
                                                           NimbleSerializeParticipantConnectionSecret connectionSecret);
struct NimbleServerParticipantConnection*
nimbleServerParticipantConnectionsFindConnectionFromParticipantId(NimbleServerParticipantConnections* self,
                                                           NimbleSerializeParticipantId participantId);

int nimbleServerParticipantConnectionsCreate(NimbleServerParticipantConnections* self,
                                             NimbleServerParticipants* gameParticipants,
                                             struct NimbleServerTransportConnection* transportConnection,
                                             const NimbleSerializeJoinGameRequestPlayer* joinInfo,
                                             StepId latestAuthoritativeStepId, size_t localParticipantCount,
                                             struct NimbleServerParticipantConnection** outConnection);

int nimbleServerParticipantConnectionsPrepare(NimbleServerParticipantConnections* self,
                                              NimbleServerParticipants* gameParticipants,
                                              StepId latestAuthoritativeStepId, NimbleSerializeParticipantId participantId,
                                              struct NimbleServerParticipantConnection** outConnection);
#endif
