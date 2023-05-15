/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#ifndef NIMBLE_SERVER_REQ_DOWNLOAD_GAME_STATE_H
#define NIMBLE_SERVER_REQ_DOWNLOAD_GAME_STATE_H

#include <nimble-serialize/version.h>

struct NimbleServerTransportConnection;
struct ImprintAllocator;
struct NimbleServerGame;
struct FldOutStream;
struct FldInStream;

int nbdReqDownloadGameState(struct NimbleServerTransportConnection* transportConnection, struct ImprintAllocator* pageAllocator,
                            const struct NimbleServerGame* game, NimbleSerializeVersion applicationVersion,
                            struct FldInStream* inStream, struct FldOutStream* outStream);

#endif
