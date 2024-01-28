/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <clog/clog.h>
#include <flood/in_stream.h>
#include <flood/out_stream.h>
#include <nimble-serialize/server_in.h>
#include <nimble-serialize/server_out.h>
#include <nimble-server/errors.h>
#include <nimble-server/participant_connection.h>
#include <nimble-server/req_connect.h>
#include <nimble-server/server.h>

int nimbleServerReqConnect(NimbleServer* self, NimbleServerTransportConnection* transportConnection,
                           FldInStream* inStream, FldOutStream* outStream)
{
    NimbleSerializeConnectRequest connectOptions;
    int serializeErr = nimbleSerializeServerInConnectRequest(inStream, &connectOptions);
    if (serializeErr < 0) {
        return NimbleServerErrSerializeVersion;
    }

    transportConnection->useDebugStreams = connectOptions.useDebugStreams;

    if (!nimbleSerializeVersionIsEqual(&self->applicationVersion, &connectOptions.applicationVersion)) {
        CLOG_SOFT_ERROR("Wrong application version")
        return NimbleServerErrSerializeVersion;
    }

    if (transportConnection->phase == NbTransportConnectionPhaseWaitingForValidConnect) {
        transportConnection->phase = NbTransportConnectionPhaseConnected;
    }

    NimbleSerializeConnectResponse connectResponse;
    connectResponse.useDebugStreams = transportConnection->useDebugStreams;

    return nimbleSerializeServerOutConnectResponse(outStream, &connectResponse, &self->log);
}
