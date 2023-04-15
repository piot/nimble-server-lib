/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#ifndef NIMBLE_SERVER_REQ_DOWNLOAD_GAME_STATE_H
#define NIMBLE_SERVER_REQ_DOWNLOAD_GAME_STATE_H

struct NbdParticipantConnection;
struct ImprintAllocator;
struct NbdGameState;
struct FldOutStream;

int nbdReqDownloadGameState(NbdParticipantConnection* foundParticipantConnection, struct ImprintAllocator* pageAllocator, struct NbdGameState* latestState, const uint8_t* data, size_t len, struct FldOutStream* outStream);

#endif

