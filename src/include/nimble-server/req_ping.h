/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/nimble-server-lib
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/

#ifndef NIMBLE_SERVER_REQ_PING_H
#define NIMBLE_SERVER_REQ_PING_H

#include <stddef.h>
#include <stdint.h>

struct FldOutStream;
struct FldInStream;
struct Clog;

int nimbleServerReqPing(struct FldInStream* inStream, struct FldOutStream* outStream, struct Clog* log);

#endif
