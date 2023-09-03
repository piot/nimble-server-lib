/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#ifndef NIMBLE_SERVER_SERVER_H
#define NIMBLE_SERVER_SERVER_H

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
#include <datagram-transport/multi.h>
#include <imprint/tagged_allocator.h>
#include <nimble-serialize/serialize.h>
#include <nimble-server/participants.h>
#include <nimble-server/transport_connection.h>
#include <nimble-steps/steps.h>
#include <stats/stats_per_second.h>
#include <stdbool.h>

struct ImprintAllocatorWithFree;
struct ImprintAllocator;
struct NimbleServerParticipant;

#define NIMBLE_NIMBLE_SERVER_MAX_TRANSPORT_CONNECTIONS 64

typedef struct NimbleServerSetup {
    NimbleSerializeVersion applicationVersion;
    struct ImprintAllocator* memory;
    struct ImprintAllocatorWithFree* blobAllocator;
    size_t maxConnectionCount;
    size_t maxParticipantCount;
    size_t maxSingleParticipantStepOctetCount;
    size_t maxParticipantCountForEachConnection;
    size_t maxWaitingForReconnectTicks;
    size_t maxGameStateOctetCount;
    const uint8_t* zeroInputOctets;
    size_t zeroInputOctetCount;
    DatagramTransportMulti multiTransport;
    MonotonicTimeMs now;
    Clog log;
} NimbleServerSetup;

typedef struct NimbleServer {
    NimbleServerTransportConnection transportConnections[NIMBLE_NIMBLE_SERVER_MAX_TRANSPORT_CONNECTIONS];

    NimbleServerParticipantConnections connections;
    NimbleServerGame game;

    struct ImprintAllocator* pageAllocator;
    struct ImprintAllocatorWithFree* blobAllocator;

    NimbleSerializeVersion applicationVersion;
    Clog log;

    DatagramTransportMulti multiTransport;
    NimbleServerSetup setup;
    uint16_t statsCounter;
    StatsIntPerSecond authoritativeStepsPerSecondStat;
} NimbleServer;

typedef struct NimbleServerResponse {
    struct DatagramTransportOut* transportOut;
} NimbleServerResponse;

int nimbleServerInit(NimbleServer* self, NimbleServerSetup setup);
int nimbleServerReInitWithGame(NimbleServer* self, const uint8_t* gameState, size_t gameStateOctetCount, StepId stepId,
                               MonotonicTimeMs now);
void nimbleServerReset(NimbleServer* self);
int nimbleServerFeed(NimbleServer* self, uint8_t connectionIndex, const uint8_t* data, size_t len,
                     NimbleServerResponse* response);
int nimbleServerReadFromMultiTransport(NimbleServer* self);
int nimbleServerUpdate(NimbleServer* self, MonotonicTimeMs now);
bool nimbleServerMustProvideGameState(const NimbleServer* self);
void nimbleServerSetGameState(NimbleServer* self, const uint8_t* gameState, size_t gameStateOctetCount, StepId stepId);
int nimbleServerConnectionConnected(NimbleServer* self, uint8_t connectionIndex);
int nimbleServerConnectionDisconnected(NimbleServer* self, uint8_t connectionIndex);

#endif
