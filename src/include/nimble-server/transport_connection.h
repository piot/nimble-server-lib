/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/nimble-server-lib
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/

#ifndef NIMBLE_SERVER_TRANSPORT_CONNECTION_H
#define NIMBLE_SERVER_TRANSPORT_CONNECTION_H

#include <blob-stream/blob_stream_logic_in.h>
#include <blob-stream/blob_stream_logic_out.h>
#include <clog/clog.h>
#include <imprint/tagged_allocator.h>
#include <nimble-serialize/serialize.h>
#include <nimble-serialize/version.h>
#include <nimble-server/game.h>
#include <nimble-server/local_parties.h>
#include <nimble-server/participants.h>
#include <nimble-steps/steps.h>
#include <ordered-datagram/in_logic.h>
#include <ordered-datagram/out_logic.h>
#include <stats/stats.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>

struct FldOutStream;

typedef enum NimbleServerTransportConnectionPhase {
    NbTransportConnectionPhaseIdle,
    NbTransportConnectionPhaseWaitingForValidConnect,
    NbTransportConnectionPhaseConnected,
    NbTransportConnectionPhaseInitialStateDetermined,
    NbTransportConnectionPhaseDisconnected
} NimbleServerTransportConnectionPhase;

typedef struct NimbleServerTransportConnection {
    uint8_t id;
    uint8_t transportConnectionId;
    uint8_t transportIndex;
    NimbleSerializeClientRequestId connectedFromConnectRequestId;
    uint64_t secret;
    struct NimbleServerLocalParty* assignedParty;
    OrderedDatagramInLogic orderedDatagramInLogic;
    OrderedDatagramOutLogic orderedDatagramOutLogic;

    BlobStreamOut blobStreamOut;
    BlobStreamLogicOut blobStreamLogicOut;
    BlobStreamTransferId nextBlobStreamOutChannel;
    uint8_t blobStreamOutClientRequestId;
    ImprintAllocatorWithFree* blobStreamOutAllocator;
    StatsInt stepsBehindStats;
    size_t debugCounter;
    Clog log;
    bool isUsed;
    bool useDebugStreams;
    uint8_t noRangesToSendCounter;
    NimbleServerTransportConnectionPhase phase;
    NimbleServerGameState gameState;
} NimbleServerTransportConnection;

void transportConnectionInit(NimbleServerTransportConnection* self, ImprintAllocatorWithFree* blobStreamAllocator,
                             size_t maxGameOctetSize, Clog log);
void transportConnectionDisconnect(NimbleServerTransportConnection* self);
void transportConnectionSetGameStateTickId(NimbleServerTransportConnection* self);
int transportConnectionWriteHeader(NimbleServerTransportConnection* self, struct FldOutStream* outStream, uint16_t clientTime);
void transportConnectionCommitHeader(NimbleServerTransportConnection* self);

#endif
