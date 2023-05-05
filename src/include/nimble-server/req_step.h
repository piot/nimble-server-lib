/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#ifndef NIMBLE_SERVER_REQ_STEP_H
#define NIMBLE_SERVER_REQ_STEP_H

#include <stddef.h>
#include <stdint.h>
#include <stats/stats_per_second.h>

struct NbdGame;
struct NbdParticipantConnection;
struct NbdParticipantConnections;
struct NbdTransportConnection;
struct FldOutStream;
struct FldInStream;

int nbdReqGameStep(struct NbdGame* game, struct NbdTransportConnection* transportConnection, StatsIntPerSecond* authoritativeStepsPerSecondStat,
                   struct NbdParticipantConnections* connections, struct FldInStream* inStream,
                   struct FldOutStream* response);

#endif
