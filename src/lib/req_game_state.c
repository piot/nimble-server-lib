/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <clog/clog.h>
#include <flood/in_stream.h>
#include <nimble-serialize/server_out.h>
#include <nimble-server/game.h>
#include <nimble-server/participant_connection.h>
#include <nimble-server/req_download_game_state.h>

int nbdReqDownloadGameState(NbdParticipantConnection* foundParticipantConnection, ImprintAllocator* pageAllocator,
                            NbdGameState* latestState, FldInStream* inStream, FldOutStream* outStream)
{
    if (latestState->octetCount == 0) {
        CLOG_NOTICE("Can not join room, game octet count in state is zero in room: %u", foundParticipantConnection->id);
        return -2;
    }

    SerializeGameState outGameState;
    outGameState.stepId = latestState->stepId;
    outGameState.gameStateOctetCount = latestState->octetCount;
    outGameState.gameState = latestState->state;

    blobStreamOutInit(&foundParticipantConnection->blobStreamOut, pageAllocator,
                      foundParticipantConnection->blobStreamOutAllocator, outGameState.gameState,
                      outGameState.gameStateOctetCount, BLOB_STREAM_CHUNK_SIZE, foundParticipantConnection->log);
    blobStreamLogicOutInit(&foundParticipantConnection->blobStreamLogicOut, &foundParticipantConnection->blobStreamOut);

    foundParticipantConnection->blobStreamOutChannel = foundParticipantConnection->nextBlobStreamOutChannel++;

    CLOG_DEBUG("server: rejoined connection %d with blobStreamChannel %02X octetCount:%zu",
               foundParticipantConnection->id, foundParticipantConnection->blobStreamOutChannel,
               outGameState.gameStateOctetCount);

    return nimbleSerializeServerOutGameStateResponse(outStream, outGameState,
                                                     foundParticipantConnection->blobStreamOutChannel);
}
