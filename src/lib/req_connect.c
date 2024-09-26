/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/nimble-server-lib
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/

#include <clog/clog.h>
#include <flood/in_stream.h>
#include <flood/out_stream.h>
#include <inttypes.h>
#include <nimble-serialize/server_in.h>
#include <nimble-serialize/server_out.h>
#include <nimble-server/errors.h>
#include <nimble-server/local_party.h>
#include <nimble-server/req_connect.h>
#include <nimble-server/server.h>
#include <secure-random/secure_random.h>

// TODO: Also check the time since the connection was last requested

static NimbleServerTransportConnection*
findExistingConnectionRequest(NimbleServer* self, uint8_t transportConnectionIndex, NimbleSerializeClientRequestId connectionRequestId)
{
    for (size_t i = 0; i < NIMBLE_NIMBLE_SERVER_MAX_TRANSPORT_CONNECTIONS; ++i) {
        NimbleServerTransportConnection* connection = &self->transportConnections[i];
        if (!connection->isUsed) {
            continue;
        }
        if (connection->transportIndex == transportConnectionIndex &&
            connection->connectedFromConnectRequestId == connectionRequestId) {
            return connection;
        }
    }
    return 0;
}

int nimbleServerReqConnect(NimbleServer* self, uint8_t transportConnectionIndex, FldInStream* inStream,
                           FldOutStream* outStream)
{
    NimbleSerializeConnectRequest connectOptions;
    int serializeErr = nimbleSerializeServerInConnectRequest(inStream, &connectOptions);
    if (serializeErr < 0) {
        return NimbleServerErrSerializeVersion;
    }

    if (!nimbleSerializeVersionIsEqual(&self->applicationVersion, &connectOptions.applicationVersion)) {
        CLOG_SOFT_ERROR("Wrong application version")
        return NimbleServerErrSerializeVersion;
    }

    // TODO: Also check the time since the connection was last requested
    NimbleServerTransportConnection* transportConnection = findExistingConnectionRequest(self, transportConnectionIndex,
                                                                                         connectOptions.clientRequestId);
    if (!transportConnection) {
        CLOG_C_DEBUG(&self->log, "request for a new connection")
        if (nimbleServerCircularBufferIsEmpty(&self->freeTransportConnectionList)) {
            CLOG_C_NOTICE(&self->log, "no free transport connection")
            return NimbleServerErrSerialize;
        }

        uint8_t freeTransportIndex = nimbleServerCircularBufferRead(&self->freeTransportConnectionList);
        transportConnection = &self->transportConnections[freeTransportIndex];
        if (transportConnection->isUsed) {
            CLOG_C_ERROR(&self->log, "the transport index from free list was not free")
            // return -2;
        }

        transportConnection->isUsed = true;
        transportConnection->transportIndex = transportConnectionIndex;
        transportConnection->connectedFromConnectRequestId = connectOptions.clientRequestId;
        transportConnection->secret = secureRandomUInt64();
        transportConnection->useDebugStreams = connectOptions.useDebugStreams;
        transportConnection->phase = NbTransportConnectionPhaseConnected;
        transportConnection->id = freeTransportIndex;

        transportConnectionInit(transportConnection, self->blobAllocator, self->setup.maxGameStateOctetCount,
                                self->log);

    } else {
        CLOG_C_DEBUG(&self->log, "return existing connection with client request id %02X", connectOptions.clientRequestId)
    }

    NimbleSerializeConnectResponse connectResponse;
    connectResponse.useDebugStreams = transportConnection->useDebugStreams;
    connectResponse.connectionId = transportConnection->id;
    connectResponse.responseToRequestId = connectOptions.clientRequestId;

    return nimbleSerializeServerOutConnectResponse(outStream, &connectResponse, &self->log);
}
