/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#ifndef NIMBLE_SERVER_SEND_STEPS_H
#define NIMBLE_SERVER_SEND_STEPS_H

#include <nimble-steps/steps.h>
#include <stddef.h>
#include <stdint.h>

struct NimbleServerGame;
struct NimbleServerTransportConnection;
struct FldOutStream;

ssize_t nimbleServerSendStepRanges(struct FldOutStream* outStream,
                               struct NimbleServerTransportConnection* transportConnection,
                               struct NimbleServerGame* foundGame, StepId clientWaitingForStepId, uint64_t receiveMask);

#endif
