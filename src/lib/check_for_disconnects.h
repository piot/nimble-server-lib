/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/nimble-server-lib
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/

#ifndef NIMBLE_SERVER_CHECK_FOR_DISCONNECTS_H
#define NIMBLE_SERVER_CHECK_FOR_DISCONNECTS_H

#include <stddef.h>
#include <stdint.h>

struct NimbleServerLocalParties;

void nimbleServerCheckForDisconnections(struct NimbleServerLocalParties* connections);

#endif
