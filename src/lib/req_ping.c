/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/nimble-server-lib
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/

#include <clog/clog.h>
#include <flood/in_stream.h>
#include <flood/out_stream.h>
#include <nimble-server/req_ping.h>
#include <nimble-serialize/server_in.h>
#include <nimble-serialize/server_out.h>
#include <nimble-server/errors.h>

int nimbleServerReqPing(FldInStream* inStream, FldOutStream* outStream, Clog* log)
{
    NimbleSerializePingRequest pingRequest;
    int serializeErr = nimbleSerializeServerInPingRequest(inStream, &pingRequest);
    if (serializeErr < 0) {
        return NimbleServerErrSerialize;
    }

    NimbleSerializePongResponse connectResponse;
    connectResponse.clientTime = pingRequest.clientTime;

    return nimbleSerializeServerOutPongResponse(outStream, &connectResponse, log);
}
