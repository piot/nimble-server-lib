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

struct NbdParticipant;

typedef struct NbdParticipantReferences {
    size_t participantReferenceCount;
    struct NbdParticipant* participantReferences[MAX_LOCAL_PLAYERS];
} NbdParticipantReferences;

/// Represents a UDP "connection" from a client which can hold several game participants. */
typedef struct NbdParticipantConnection {
    uint32_t id;
    bool isUsed;
    NbsSteps steps;
    size_t debugCounter;
    NbdParticipantReferences participantReferences;
    StatsInt stepsBehindStats;
    StatsInt incomingStepCountInBufferStats;
    StepId nextAuthoritativeStepIdToSend;
    BlobStreamOut blobStreamOut;
    BlobStreamLogicOut blobStreamLogicOut;
    NimbleSerializeBlobStreamChannelId blobStreamOutChannel;
    NimbleSerializeBlobStreamChannelId nextBlobStreamOutChannel;
    size_t transportConnectionId;
    ImprintAllocatorWithFree* blobStreamOutAllocator;
    ImprintAllocator* allocatorWithNoFree;
    uint8_t noRangesToSendCounter;
    Clog log;
} NbdParticipantConnection;

void nbdParticipantConnectionInit(NbdParticipantConnection* self, size_t transportConnectionId,
                                  ImprintAllocator* allocator, ImprintAllocatorWithFree* blobAllocator,
                                  size_t maxParticipantCountForConnection, size_t maxSingleParticipantStepOctetCount,
                                  Clog log);

void nbdParticipantConnectionReInit(NbdParticipantConnection* self, StepId stepId,
                                    const struct NbdParticipant** participants, size_t participantCount);

void nbdParticipantConnectionReset(NbdParticipantConnection* self);

int nbdParticipantConnectionHasParticipantId(const NbdParticipantConnection* self, uint8_t participantId);

#endif
