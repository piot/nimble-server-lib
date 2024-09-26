/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/nimble-server-lib
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/

#ifndef NIMBLE_SERVER_SERVER_H
#define NIMBLE_SERVER_SERVER_H

#include <clog/clog.h>
#include <datagram-transport/multi.h>
#include <nimble-serialize/version.h>
#include <nimble-server/game.h>
#include <nimble-server/local_parties.h>
#include <nimble-server/serialized_game_state.h>
#include <nimble-server/transport_connection.h>
#include <nimble-server/update_quality.h>
#include <nimble-steps/steps.h>
#include <stats/stats_per_second.h>
#include <stdbool.h>
#include <stdint.h>

struct ImprintAllocatorWithFree;
struct ImprintAllocator;
struct NimbleServerParticipant;

#define NIMBLE_NIMBLE_SERVER_MAX_TRANSPORT_CONNECTIONS 64

typedef void (*NimbleServerSerializeStateFn)(void* self, NimbleServerSerializedGameState* state);

typedef struct NimbleServerCallbackObjectVtbl {
    NimbleServerSerializeStateFn authoritativeStateSerializeFn;
} NimbleServerCallbackObjectVtbl;

typedef struct NimbleServerCallbackObject {
    NimbleServerCallbackObjectVtbl* vtbl;
    void* self;
} NimbleServerCallbackObject;

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
    NimbleServerCallbackObject callbackObject;
    DatagramTransportMulti multiTransport;
    MonotonicTimeMs now;
    size_t targetTickTimeMs;
    Clog log;
} NimbleServerSetup;

typedef struct NimbleServer {
    NimbleServerTransportConnection transportConnections[NIMBLE_NIMBLE_SERVER_MAX_TRANSPORT_CONNECTIONS];
    NimbleServerLocalParties localParties;
    NimbleServerGame game;
    struct ImprintAllocator* pageAllocator;
    struct ImprintAllocatorWithFree* blobAllocator;
    NimbleSerializeVersion applicationVersion;
    Clog log;
    DatagramTransportMulti multiTransport;
    NimbleServerSetup setup;
    uint16_t statsCounter;
    StatsIntPerSecond authoritativeStepsPerSecondStat;
    NimbleServerUpdateQuality updateQuality;
    NimbleServerCallbackObject callbackObject;

    NimbleServerCircularBuffer freeTransportConnectionList;
    NimbleSerializeSessionSecret sessionSecret;
} NimbleServer;

typedef struct NimbleServerResponse {
    struct DatagramTransportOut* transportOut;
} NimbleServerResponse;

int nimbleServerInit(NimbleServer* self, NimbleServerSetup setup);
int nimbleServerHostMigration(NimbleServer* self,NimbleSerializeLocalPartyInfo localPartyInfos[],
                              size_t localPartyCount);
int nimbleServerReInitWithGame(NimbleServer* self, StepId stepId, MonotonicTimeMs now);
void nimbleServerReset(NimbleServer* self);
int nimbleServerFeed(NimbleServer* self, uint8_t connectionIndex, const uint8_t* data, size_t len,
                     NimbleServerResponse* response);
int nimbleServerReadFromMultiTransport(NimbleServer* self);
int nimbleServerUpdate(NimbleServer* self, MonotonicTimeMs now);
bool nimbleServerMustProvideGameState(const NimbleServer* self);
void nimbleServerSetGameState(NimbleServer* self, const uint8_t* gameState, size_t gameStateOctetCount, StepId stepId);
int nimbleServerConnectionConnected(NimbleServer* self, uint8_t connectionIndex);
int nimbleServerConnectionDisconnected(NimbleServer* self, uint8_t connectionIndex);
bool nimbleServerIsErrorExternal(int err);

#endif
