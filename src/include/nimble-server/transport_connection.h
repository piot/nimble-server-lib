/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#ifndef NIMBLE_SERVER_TRANSPORT_CONNECTION_H
#define NIMBLE_SERVER_TRANSPORT_CONNECTION_H

#include <clog/clog.h>
#include <nimble-serialize/version.h>
#include <nimble-server/game.h>
#include <nimble-server/participant_connections.h>
#include <ordered-datagram/in_logic.h>
#include <ordered-datagram/out_logic.h>
#include <stdarg.h>
#include <stdint.h>

#include <blob-stream/blob_stream_logic_in.h>
#include <blob-stream/blob_stream_logic_out.h>
#include <imprint/tagged_allocator.h>
#include <nimble-serialize/serialize.h>
#include <nimble-server/participants.h>
#include <nimble-steps/steps.h>
#include <stats/stats.h>
#include <stdbool.h>

typedef enum NbdTransportConnectionPhase {
    NbTransportConnectionPhaseIdle,
    NbTransportConnectionPhaseInitialStateDetermined
} NbdTransportConnectionPhase;

typedef struct NbdTransportConnection {
    uint8_t transportConnectionId;
    struct NbdParticipantConnection* assignedParticipantConnection;
    OrderedDatagramInLogic orderedDatagramInLogic;
    OrderedDatagramOutLogic orderedDatagramOutLogic;
    BlobStreamOut blobStreamOut;
    BlobStreamLogicOut blobStreamLogicOut;
    NimbleSerializeBlobStreamChannelId blobStreamOutChannel;
    NimbleSerializeBlobStreamChannelId nextBlobStreamOutChannel;
    uint8_t blobStreamOutClientRequestId;
    ImprintAllocatorWithFree* blobStreamOutAllocator;
    StatsInt stepsBehindStats;
    size_t debugCounter;
    Clog log;
    bool isUsed;
    StepId nextAuthoritativeStepIdToSend;
    uint8_t noRangesToSendCounter;
    NbdTransportConnectionPhase phase;
} NbdTransportConnection;

void transportConnectionInit(NbdTransportConnection* self, ImprintAllocatorWithFree* blobStreamAllocator, Clog log);

void transportConnectionSetGameStateTickId(NbdTransportConnection* self, StepId lastAuthoritativeStateTickId);
#endif
