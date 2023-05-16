/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#ifndef NIMBLE_SERVER_TRANSPORT_CONNECTION_STATS_H
#define NIMBLE_SERVER_TRANSPORT_CONNECTION_STATS_H

#include <stats/stats_per_second.h>
#include <stddef.h>
#include <stdint.h>
#include <nimble-steps/steps.h>

struct NimbleServerGame;
struct NimbleServerParticipantConnection;
struct NimbleServerParticipantConnections;
struct NimbleServerTransportConnection;
struct FldOutStream;
struct FldInStream;

void nimbleServerTransportConnectionUpdateStats(struct NimbleServerTransportConnection* transportConnection,
                                                struct NimbleServerGame* foundGame, StepId clientWaitingForStepId);

#endif
