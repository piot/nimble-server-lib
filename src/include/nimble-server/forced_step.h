/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#ifndef NIMBLE_SERVER_FORCED_STEP_H
#define NIMBLE_SERVER_FORCED_STEP_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include <nimble-steps-serialize/types.h>

struct NimbleServerParticipantConnection;

int nimbleServerInsertForcedSteps(struct NimbleServerParticipantConnection* foundParticipantConnection,
                                  size_t dropped);
ssize_t nimbleServerCreateForcedStep(struct NimbleServerParticipantConnection* connection,
                                     uint8_t* defaultStepBuffer,
                                     size_t maxCount);

#endif
