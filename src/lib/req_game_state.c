/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include "nimble-server/server.h"
#include <flood/in_stream.h>
#include <nimble-serialize/server_out.h>
#include <nimble-server/participant_connection.h>
#include <nimble-server/req_download_game_state.h>

/// Handles a request from the client to download the latest game state.
/// @param transportConnection transport connection that request to download the latest game state
/// @param inStream stream to read the request from
/// @param outStream out stream to write the response to
/// @return negative on error
int nimbleServerReqDownloadGameState(NimbleServer* self, NimbleServerTransportConnection* transportConnection,
                                     FldInStream* inStream, FldOutStream* outStream)
{
    NimbleServerSerializedGameState serializedGameState;

    self->callbackObject.vtbl->authoritativeStateSerializeFn(self->callbackObject.self, &serializedGameState);

    nimbleServerGameStateSet(&transportConnection->gameState, serializedGameState.stepId, serializedGameState.gameState,
                             serializedGameState.gameStateOctetCount, &transportConnection->log);

    const NimbleServerGameState* latestState = &transportConnection->gameState;

    uint8_t downloadClientRequestId;
    fldInStreamReadUInt8(inStream, &downloadClientRequestId);

    SerializeGameState outGameState;
    outGameState.stepId = latestState->stepId;
    outGameState.gameStateOctetCount = latestState->octetCount;
    outGameState.gameState = latestState->state;

    if (transportConnection->blobStreamOutClientRequestId != downloadClientRequestId) {
        blobStreamOutInit(&transportConnection->blobStreamOut, self->pageAllocator,
                          transportConnection->blobStreamOutAllocator, outGameState.gameState,
                          outGameState.gameStateOctetCount, BLOB_STREAM_CHUNK_SIZE, transportConnection->log);
        blobStreamLogicOutInit(&transportConnection->blobStreamLogicOut, &transportConnection->blobStreamOut);

        transportConnection->blobStreamOutChannel = ++transportConnection->nextBlobStreamOutChannel;
        transportConnection->blobStreamOutClientRequestId = downloadClientRequestId;
        transportConnectionSetGameStateTickId(transportConnection);

        CLOG_C_DEBUG(
            &transportConnection->log,
            "start download state for connection %d, requestId %02X with blobStreamChannel %02X octetCount:%zu",
            transportConnection->transportConnectionId, transportConnection->blobStreamOutClientRequestId,
            transportConnection->blobStreamOutChannel, outGameState.gameStateOctetCount)
    } else {
        CLOG_C_VERBOSE(
            &transportConnection->log,
            "download request reply for for connection %d, requestId %02X with blobStreamChannel %02X octetCount:%zu",
            transportConnection->transportConnectionId, transportConnection->blobStreamOutClientRequestId,
            transportConnection->blobStreamOutChannel, outGameState.gameStateOctetCount)
    }

    return nimbleSerializeServerOutGameStateResponse(
        outStream, outGameState, transportConnection->blobStreamOutClientRequestId,
        transportConnection->blobStreamOutChannel, &transportConnection->log);
}
