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
/// @param pageAllocator page allocator
/// @param game game that the connection wants to download state from
/// @param inStream stream to read the request from
/// @param outStream out stream to write the response to
/// @return negative on error
int nimbleServerReqDownloadGameState(NimbleServerTransportConnection* transportConnection, ImprintAllocator* pageAllocator,
                            const NimbleServerGame* game, FldInStream* inStream,
                            FldOutStream* outStream)
{
    const NimbleServerGameState* _latestState = &game->latestState;
    if (_latestState->octetCount == 0) {
        CLOG_NOTICE("Can not join room, game octet count in state is zero in room: %u",
                    transportConnection->transportConnectionId)
        return -2;
    }

    nimbleServerGameStateCopy(&transportConnection->gameState, _latestState, &transportConnection->log);

    const NimbleServerGameState* latestState = &transportConnection->gameState;

    uint8_t downloadClientRequestId;
    fldInStreamReadUInt8(inStream, &downloadClientRequestId);

    SerializeGameState outGameState;
    outGameState.stepId = latestState->stepId;
    outGameState.gameStateOctetCount = latestState->octetCount;
    outGameState.gameState = latestState->state;

    if (transportConnection->blobStreamOutClientRequestId != downloadClientRequestId) {
        blobStreamOutInit(&transportConnection->blobStreamOut, pageAllocator,
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

    return nimbleSerializeServerOutGameStateResponse(outStream, outGameState,
                                                     transportConnection->blobStreamOutClientRequestId,
                                                     transportConnection->blobStreamOutChannel, &transportConnection->log);
}
