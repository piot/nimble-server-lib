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
#include <nimble-server/game.h>
#include <nimble-server/participant.h>
#include <nimble-server/participant_connection.h>
#include <nimble-server/req_connect.h>
#include <nimble-server/server.h>

/*
    NimbleSerializeVersion nimbleProtocolVersion;
    int errorCode = nimbleSerializeInVersion(inStream, &nimbleProtocolVersion);
    if (errorCode < 0) {
        CLOG_SOFT_ERROR("could not read nimble serialize version %d", errorCode)
        return errorCode;
    }

    char buf[32];
    CLOG_C_VERBOSE(&transportConnection->log, "request game state. nimble protocol version %s",
                   nimbleSerializeVersionToString(&nimbleProtocolVersion, buf, 32))

    if (!nimbleSerializeVersionIsEqual(&nimbleProtocolVersion, &g_nimbleProtocolVersion)) {

        CLOG_SOFT_ERROR("wrong version of nimble protocol version. expected %s, but encountered %s",
                        nimbleSerializeVersionToString(&g_nimbleProtocolVersion, buf, 32),
                        nimbleSerializeVersionToString(&nimbleProtocolVersion, buf, 32))
        return -41;
    }

    NimbleSerializeVersion clientApplicationVersion;
    errorCode = nimbleSerializeInVersion(inStream, &clientApplicationVersion);
    if (errorCode < 0) {
        CLOG_SOFT_ERROR("wrong version %d", errorCode)
        return errorCode;
    }

    CLOG_C_VERBOSE(&transportConnection->log, "request game state. application version version %s",
                   nimbleSerializeVersionToString(&clientApplicationVersion, buf, 32))

    if (!nimbleSerializeVersionIsEqual(&applicationVersion, &clientApplicationVersion)) {
        CLOG_SOFT_ERROR("Wrong application version")
        return -44;
    }

*/

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

    return nimbleSerializeServerOutConnectResponse(outStream, &connectResponse);
}
