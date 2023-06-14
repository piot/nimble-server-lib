/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#ifndef NIMBLE_SERVER_TRANSPORT_CONNECTION_H
#define NIMBLE_SERVER_TRANSPORT_CONNECTION_H

#include <blob-stream/blob_stream_logic_in.h>
#include <blob-stream/blob_stream_logic_out.h>
#include <clog/clog.h>
#include <imprint/tagged_allocator.h>
#include <nimble-serialize/serialize.h>
#include <nimble-serialize/version.h>
#include <nimble-server/game.h>
#include <nimble-server/participant_connections.h>
#include <nimble-server/participants.h>
#include <nimble-steps/steps.h>
#include <ordered-datagram/in_logic.h>
#include <ordered-datagram/out_logic.h>
#include <stats/stats.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

typedef enum NimbleServerTransportConnectionPhase {
    NbTransportConnectionPhaseIdle,
    NbTransportConnectionPhaseInitialStateDetermined,
    NbTransportConnectionPhaseDisconnected
} NimbleServerTransportConnectionPhase;

typedef struct NimbleServerTransportConnection {
    uint8_t transportConnectionId;
    struct NimbleServerParticipantConnection* assignedParticipantConnection;
    OrderedDatagramInLogic orderedDatagramInLogic;
    OrderedDatagramOutLogic orderedDatagramOutLogic;
    BlobStreamOut blobStreamOut;
    BlobStreamLogicOut blobStreamLogicOut;
    NimbleSerializeBlobStreamChannelId blobStreamOutChannel;
    NimbleSerializeBlobStreamChannelId nextBlobStreamOutChannel;
    uint8_t blobStreamOutClientRequestId;
    StatsInt stepsBehindStats;
    size_t debugCounter;
    Clog log;
    bool isUsed;
    uint8_t noRangesToSendCounter;
    NimbleServerTransportConnectionPhase phase;
} NimbleServerTransportConnection;

void transportConnectionInit(NimbleServerTransportConnection* self, Clog log);
void transportConnectionSetGameStateTickId(NimbleServerTransportConnection* self, StepId lastAuthoritativeStateTickId);

#endif
