/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#ifndef NIMBLE_SERVER_SEND_STEPS_H
#define NIMBLE_SERVER_SEND_STEPS_H

#include <nimble-steps/steps.h>
#include <stats/stats_per_second.h>
#include <stddef.h>
#include <stdint.h>

struct NimbleServerGame;
struct NimbleServerParticipantConnection;
struct NimbleServerParticipantConnections;
struct NimbleServerTransportConnection;
struct FldOutStream;
struct FldInStream;

int nimbleServerSendStepRanges(struct FldOutStream* outStream,
                               struct NimbleServerTransportConnection* transportConnection,
                               struct NimbleServerGame* foundGame, StepId clientWaitingForStepId, uint64_t receiveMask,
                               uint16_t receivedTimeFromClient);

#endif
