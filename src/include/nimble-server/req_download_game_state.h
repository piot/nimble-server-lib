/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#ifndef NIMBLE_SERVER_REQ_DOWNLOAD_GAME_STATE_H
#define NIMBLE_SERVER_REQ_DOWNLOAD_GAME_STATE_H

#include <nimble-serialize/version.h>

struct NbdTransportConnection;
struct ImprintAllocator;
struct NbdGameState;
struct FldOutStream;
struct FldInStream;

int nbdReqDownloadGameState(struct NbdTransportConnection* transportConnection, struct ImprintAllocator* pageAllocator,
                            struct NbdGameState* latestState, NimbleSerializeVersion applicationVersion,
                            struct FldInStream* inStream, struct FldOutStream* outStream);

#endif
