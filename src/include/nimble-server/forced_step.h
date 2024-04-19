/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/nimble-server-lib
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/
#ifndef NIMBLE_SERVER_FORCED_STEP_H
#define NIMBLE_SERVER_FORCED_STEP_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include <nimble-steps-serialize/types.h>

struct NimbleServerLocalParty;

int nimbleServerInsertForcedSteps(struct NimbleServerLocalParty* party,
                                  size_t dropped);
ssize_t nimbleServerCreateForcedStep(struct NimbleServerLocalParty* connection,
                                     uint8_t* defaultStepBuffer,
                                     size_t maxCount);

#endif
