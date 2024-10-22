/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/nimble-server-lib
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/

#ifndef NIMBLE_SERVER_REQ_DOWNLOAD_GAME_STATE_H
#define NIMBLE_SERVER_REQ_DOWNLOAD_GAME_STATE_H

#include "server.h"

struct NimbleServerTransportConnection;
struct ImprintAllocator;
struct NimbleServerGame;
struct FldOutStream;
struct FldInStream;
struct DatagramTransportOut;

int nimbleServerReqDownloadGameState(NimbleServer* self, struct NimbleServerTransportConnection* transportConnection,
                                     struct FldInStream* inStream, struct DatagramTransportOut* transportOut);

#endif
