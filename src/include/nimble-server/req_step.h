/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#ifndef NIMBLE_SERVER_REQ_STEP_H
#define NIMBLE_SERVER_REQ_STEP_H

#include <stats/stats_per_second.h>
#include <stddef.h>
#include <stdint.h>

struct NimbleServerGame;
struct NimbleServerParticipantConnection;
struct NimbleServerParticipantConnections;
struct NimbleServerTransportConnection;
struct FldOutStream;
struct FldInStream;

int nimbleServerReqGameStep(struct NimbleServerGame* game, struct NimbleServerTransportConnection* transportConnection,
                   StatsIntPerSecond* authoritativeStepsPerSecondStat,
                   struct NimbleServerParticipantConnections* connections, struct FldInStream* inStream,
                   struct FldOutStream* response);

#endif
