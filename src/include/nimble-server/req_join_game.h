/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <stdint.h>
#include <stddef.h>

#ifndef NIMBLE_SERVER_REQ_JOIN_GAME_H
#define NIMBLE_SERVER_REQ_JOIN_GAME_H

struct NbdServer;
struct FldOutStream;
struct NbdTransportConnection;

int nbdReqGameJoin(struct NbdServer *self, struct NbdTransportConnection *transportConnection, const uint8_t *data,
                   size_t len, struct FldOutStream *outStream);

#endif
