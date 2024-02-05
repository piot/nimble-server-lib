/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include "nimble-server/server.h"
#include <flood/in_stream.h>
#include <inttypes.h>
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
    uint8_t downloadClientRequestId;
    fldInStreamReadUInt8(inStream, &downloadClientRequestId);

    if (downloadClientRequestId == transportConnection->blobStreamOutClientRequestId) {
        CLOG_C_VERBOSE(&transportConnection->log,
                       "already sent download game state response. resending same information again. connection %d, "
                       "requestId %02X with blobStreamChannel %02X octetCount:%zu",
                       transportConnection->transportConnectionId, transportConnection->blobStreamOutClientRequestId,
                       transportConnection->blobStreamOutChannel, transportConnection->gameState.octetCount)

    } else {
        /// Fetch state and copy it to the transport connection
        /// Initialize the outgoing blob stream with the state
        {
            NimbleServerSerializedGameState serializedGameState;

            self->callbackObject.vtbl->authoritativeStateSerializeFn(self->callbackObject.self, &serializedGameState);

            CLOG_C_VERBOSE(&self->log, "download game state request stepId:%04X octetSize:%zu, hash:%08" PRIX64,
                        serializedGameState.stepId, serializedGameState.gameStateOctetCount, serializedGameState.hash)

            nimbleServerGameStateSet(&transportConnection->gameState, serializedGameState.stepId,
                                     serializedGameState.gameState, serializedGameState.gameStateOctetCount,
                                     &transportConnection->log);
        }

        {
            const NimbleServerGameState* copiedGameState = &transportConnection->gameState;

            blobStreamOutInit(&transportConnection->blobStreamOut, self->pageAllocator,
                              transportConnection->blobStreamOutAllocator, copiedGameState->state,
                              copiedGameState->octetCount, BLOB_STREAM_CHUNK_SIZE, transportConnection->log);
            blobStreamLogicOutInit(&transportConnection->blobStreamLogicOut, &transportConnection->blobStreamOut);

            transportConnection->blobStreamOutChannel = ++transportConnection->nextBlobStreamOutChannel;
            transportConnection->blobStreamOutClientRequestId = downloadClientRequestId;
            transportConnectionSetGameStateTickId(transportConnection);

            CLOG_C_DEBUG(
                &transportConnection->log,
                "start download state for connection %d, requestId %02X with blobStreamChannel %02X octetCount:%zu",
                transportConnection->transportConnectionId, transportConnection->blobStreamOutClientRequestId,
                transportConnection->blobStreamOutChannel, copiedGameState->octetCount)
        }
    }

    // No matter if it is a resend or first time response, send out the information we have
    // in the transport connection
    const NimbleServerGameState* latestState = &transportConnection->gameState;

    SerializeGameState outGameState;
    outGameState.stepId = latestState->stepId;
    outGameState.gameStateOctetCount = latestState->octetCount;
    outGameState.gameState = latestState->state;

    return nimbleSerializeServerOutGameStateResponse(
        outStream, outGameState, transportConnection->blobStreamOutClientRequestId,
        transportConnection->blobStreamOutChannel, &transportConnection->log);
}
