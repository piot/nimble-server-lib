/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#ifndef NIMBLE_SERVER_GAME_PARTICIPANT_H
#define NIMBLE_SERVER_GAME_PARTICIPANT_H

#include <blob-stream/blob_stream_logic_in.h>
#include <imprint/tagged_allocator.h>
#include <nimble-serialize/serialize.h>
#include <nimble-server/connection_quality.h>
#include <nimble-server/delayed_quality.h>
#include <nimble-server/participants.h>
#include <nimble-steps/steps.h>
#include <stats/stats.h>
#include <stdbool.h>

struct NimbleServerParticipant;
struct NimbleServerTransportConnection;

typedef struct NimbleServerParticipantReferences {
    size_t participantReferenceCount;
    struct NimbleServerParticipant* participantReferences[NIMBLE_SERIALIZE_MAX_LOCAL_PLAYERS];
    struct NimbleServerParticipants* gameParticipants;
} NimbleServerParticipantReferences;

typedef enum NimbleServerParticipantConnectionState {
    NimbleServerParticipantConnectionStateNormal,
    NimbleServerParticipantConnectionStateWaitingForReconnect,
    NimbleServerParticipantConnectionStateDisconnected
} NimbleServerParticipantConnectionState;

/// Represents a UDP "connection" from a client which can hold several game participants. */
typedef struct NimbleServerParticipantConnection {
    uint32_t id;
    bool isUsed;
    NbsSteps steps;

    NimbleServerParticipantConnectionState state;
    NimbleServerParticipantReferences participantReferences;

    StatsInt incomingStepCountInBufferStats;
    struct NimbleServerTransportConnection* transportConnection;
    size_t waitingForReconnectTimer;
    size_t waitingForReconnectMaxTimer;
    NimbleSerializeParticipantConnectionSecret secret;
    NimbleServerConnectionQuality quality;
    NimbleServerConnectionQualityDelayed delayedQuality;
    uint32_t warningCount;
    uint32_t warningAboutZeroAddedSteps;
    char debugPrefix[32];
    Clog log;
} NimbleServerParticipantConnection;

void nimbleServerParticipantConnectionInit(NimbleServerParticipantConnection* self,
                                           struct NimbleServerTransportConnection* transportConnection,
                                           ImprintAllocator* allocator, StepId latestAuthoritativeStepId,
                                           size_t maxParticipantCountForConnection,
                                           size_t maxSingleParticipantStepOctetCount, Clog log);
void nimbleServerParticipantConnectionReset(NimbleServerParticipantConnection* self);
void nimbleServerParticipantConnectionReInit(NimbleServerParticipantConnection* self,
                                             struct NimbleServerTransportConnection* transportConnection,
                                             StepId latestAuthoritativeStepId);
void nimbleServerParticipantConnectionRejoin(NimbleServerParticipantConnection* self,
                                             struct NimbleServerTransportConnection* transportConnection,
                                             StepId currentAuthoritativeStepId);
void nimbleServerParticipantConnectionDisconnect(NimbleServerParticipantConnection* self);
bool nimbleServerParticipantConnectionHasParticipantId(const NimbleServerParticipantConnection* self,
                                                       uint8_t participantId);
bool nimbleServerParticipantConnectionTick(NimbleServerParticipantConnection* self);
int nimbleServerParticipantConnectionDeserializePredictedSteps(NimbleServerParticipantConnection* self,
                                                               struct FldInStream* inStream);

#endif
