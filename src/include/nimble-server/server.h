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

struct ImprintAllocatorWithFree;
struct ImprintAllocator;
struct NbdParticipant;

typedef struct NbdTransportConnection {
    uint8_t transportConnectionId;
    struct NbdParticipantConnection* assignedParticipantConnection;
    OrderedDatagramInLogic orderedDatagramInLogic;
    OrderedDatagramOutLogic orderedDatagramOutLogic;
} NbdTransportConnection;

#define NBD_SERVER_MAX_TRANSPORT_CONNECTIONS 64

typedef struct NbdServerSetup {
    NimbleSerializeVersion applicationVersion;
    struct ImprintAllocator* memory;
    struct ImprintAllocatorWithFree* blobAllocator;
    size_t maxConnectionCount;
    size_t maxParticipantCount;
    size_t maxSingleParticipantStepOctetCount;
    size_t maxParticipantCountForEachConnection;
    size_t maxGameStateOctetCount;
    Clog log;
} NbdServerSetup;

typedef struct NbdServer {
    NbdTransportConnection transportConnections[NBD_SERVER_MAX_TRANSPORT_CONNECTIONS];
    NbdParticipantConnections connections;
    NbdGame game;

    struct ImprintAllocator* pageAllocator;
    struct ImprintAllocatorWithFree* blobAllocator;

    NimbleSerializeVersion applicationVersion;
    Clog log;

    NbdServerSetup setup;
} NbdServer;

typedef struct NbdResponse {
    struct UdpTransportOut* transportOut;
} NbdResponse;

int nbdServerInit(NbdServer* self, NbdServerSetup setup);
int nbdServerReInitWithGame(NbdServer* self, const uint8_t* gameState, size_t gameStateOctetCount);
void nbdServerDestroy(NbdServer* self);
void nbdServerReset(NbdServer* self);
int nbdServerFeed(NbdServer* self, uint8_t connectionIndex, const uint8_t* data, size_t len, NbdResponse* response);

int nbdServerConnectionConnected(NbdServer* self, uint8_t connectionIndex);
int nbdServerConnectionDisconnected(NbdServer* self, uint8_t connectionIndex);

#endif
