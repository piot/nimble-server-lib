/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/nimble-server-lib
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/
#ifndef NIMBLE_SERVER_REQ_CONNECT_H
#define NIMBLE_SERVER_REQ_CONNECT_H

#include <stddef.h>
#include <stdint.h>

struct NimbleServer;
struct FldOutStream;
struct FldInStream;
struct NimbleServerTransportConnection;

int nimbleServerReqConnect(struct NimbleServer* self, uint8_t transportConnectionIndex,
                           struct FldInStream* inStream, struct FldOutStream* outStream);

#endif
