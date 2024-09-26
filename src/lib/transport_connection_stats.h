/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/nimble-server-lib
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/

#ifndef NIMBLE_SERVER_TRANSPORT_CONNECTION_STATS_H
#define NIMBLE_SERVER_TRANSPORT_CONNECTION_STATS_H

#include <nimble-steps/steps.h>
#include <stddef.h>
#include <stdint.h>

struct NimbleServerTransportConnection;
struct FldOutStream;
struct FldInStream;
struct NimbleServerGame;

void nimbleServerTransportConnectionUpdateStats(struct NimbleServerTransportConnection* transportConnection,
                                                struct NimbleServerGame* foundGame, StepId clientWaitingForStepId);

#endif
