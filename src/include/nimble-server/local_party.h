/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/nimble-server-lib
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/
#ifndef NIMBLE_SERVER_GAME_PARTICIPANT_H
#define NIMBLE_SERVER_GAME_PARTICIPANT_H

#include <blob-stream/blob_stream_logic_in.h>
#include <imprint/tagged_allocator.h>
#include <nimble-serialize/serialize.h>
#include <nimble-server/connection_quality.h>
#include <nimble-server/delayed_quality.h>
#include <nimble-server/participant_references.h>
#include <nimble-server/participants.h>

#include <stats/stats.h>
#include <stdbool.h>

struct NimbleServerParticipant;
struct NimbleServerTransportConnection;

typedef enum NimbleServerLocalPartyState {
    NimbleServerLocalPartyStateNormal,
    NimbleServerLocalPartyStateWaitingForReJoin,
    NimbleServerLocalPartyStateDissolved
} NimbleServerLocalPartyState;

/// Represents the collection of participants joined from one device (one client transport connection). */
typedef struct NimbleServerLocalParty {
    NimbleSerializeLocalPartyId id;
    bool isUsed;

    NimbleServerLocalPartyState state;
    NimbleServerParticipantReferences participantReferences;

    StatsInt incomingStepCountInBufferStats;
    struct NimbleServerTransportConnection* transportConnection;
    size_t waitingForReconnectTimer;
    size_t waitingForReconnectMaxTimer;
    NimbleServerConnectionQuality quality;
    NimbleServerConnectionQualityDelayed delayedQuality;
    uint32_t warningCount;
    uint32_t warningAboutZeroAddedSteps;

    StepId highestReceivedStepId;
    size_t stepsInBufferCount;

    char debugPrefix[32];
    Clog log;
} NimbleServerLocalParty;

void nimbleServerLocalPartyInit(NimbleServerLocalParty* self,
                                struct NimbleServerTransportConnection* transportConnection, Clog log);
void nimbleServerLocalPartyReset(NimbleServerLocalParty* self);
void nimbleServerLocalPartyReInit(NimbleServerLocalParty* self,
                                  struct NimbleServerTransportConnection* transportConnection);
void nimbleServerLocalPartyRejoin(NimbleServerLocalParty* self,
                                  struct NimbleServerTransportConnection* transportConnection);
void nimbleServerLocalPartyDestroy(NimbleServerLocalParty* self);
bool nimbleServerLocalPartyHasParticipantId(const NimbleServerLocalParty* self, uint8_t participantId);
bool nimbleServerLocalPartyTick(NimbleServerLocalParty* self);
int nimbleServerLocalPartyDeserializePredictedSteps(NimbleServerLocalParty* self, struct FldInStream* inStream);

#endif
