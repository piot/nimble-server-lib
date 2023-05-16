/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#ifndef NIMBLE_SERVER_GAME_PARTICIPANT_H
#define NIMBLE_SERVER_GAME_PARTICIPANT_H

#include <blob-stream/blob_stream_logic_in.h>
#include <blob-stream/blob_stream_logic_out.h>
#include <imprint/tagged_allocator.h>
#include <nimble-serialize/serialize.h>
#include <nimble-server/participants.h>
#include <nimble-steps/steps.h>
#include <stats/stats.h>
#include <stdbool.h>

struct NimbleServerParticipant;
struct NimbleServerTransportConnection;

typedef struct NimbleServerParticipantReferences {
    size_t participantReferenceCount;
    struct NimbleServerParticipant* participantReferences[MAX_LOCAL_PLAYERS];
} NimbleServerParticipantReferences;

typedef enum NimbleServerParticipantConnectionState {
    NimbleServerParticipantConnectionStateNormal,
    NimbleServerParticipantConnectionStateImpendingDisconnect,
    NimbleServerParticipantConnectionStateImpendingDisconnected
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
    ImprintAllocator* allocatorWithNoFree;
    size_t forcedStepInRowCounter;
    size_t providedStepsInARow;
    size_t impedingDisconnectCounter;
    Clog log;
} NimbleServerParticipantConnection;

void nbdParticipantConnectionInit(NimbleServerParticipantConnection* self, struct NimbleServerTransportConnection* transportConnection,
                                  ImprintAllocator* allocator, StepId latestAuthoritativeStepId,
                                  size_t maxParticipantCountForConnection, size_t maxSingleParticipantStepOctetCount,
                                  Clog log);

void nbdParticipantConnectionReset(NimbleServerParticipantConnection* self);

bool nbdParticipantConnectionHasParticipantId(const NimbleServerParticipantConnection* self, uint8_t participantId);

#endif
