/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <stddef.h>
#include <stdint.h>

#ifndef NIMBLE_SERVER_REQ_JOIN_GAME_H
#define NIMBLE_SERVER_REQ_JOIN_GAME_H

struct NbdServer;
struct FldOutStream;
struct FldInStream;
struct NbdTransportConnection;

int nbdReqGameJoin(struct NbdServer* self, struct NbdTransportConnection* transportConnection,
                   struct FldInStream* inStream, struct FldOutStream* outStream);

#endif