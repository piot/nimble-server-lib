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
/// @param transportConnection
/// @param pageAllocator
/// @param game
/// @param applicationVersion
/// @param inStream
/// @param outStream
/// @return
int nimbleServerReqDownloadGameState(NimbleServerTransportConnection* transportConnection, ImprintAllocator* pageAllocator,
                            const NimbleServerGame* game, NimbleSerializeVersion applicationVersion, FldInStream* inStream,
                            FldOutStream* outStream)
{
    const NimbleServerGameState* latestState = &game->latestState;
    if (latestState->octetCount == 0) {
        CLOG_NOTICE("Can not join room, game octet count in state is zero in room: %u",
                    transportConnection->transportConnectionId);
        return -2;
    }

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
        CLOG_SOFT_ERROR("Wrong application version");
        return -44;
    }

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
        transportConnectionSetGameStateTickId(transportConnection, outGameState.stepId);

        CLOG_C_DEBUG(
            &transportConnection->log,
            "start download state for connection %d, requestId %02X with blobStreamChannel %02X octetCount:%zu",
            transportConnection->transportConnectionId, transportConnection->blobStreamOutClientRequestId,
            transportConnection->blobStreamOutChannel, outGameState.gameStateOctetCount);
    } else {
        CLOG_C_VERBOSE(
            &transportConnection->log,
            "download request reply for for connection %d, requestId %02X with blobStreamChannel %02X octetCount:%zu",
            transportConnection->transportConnectionId, transportConnection->blobStreamOutClientRequestId,
            transportConnection->blobStreamOutChannel, outGameState.gameStateOctetCount);
    }

    return nimbleSerializeServerOutGameStateResponse(outStream, outGameState,
                                                     transportConnection->blobStreamOutClientRequestId,
                                                     transportConnection->blobStreamOutChannel);
}
