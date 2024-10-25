/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/nimble-server-lib
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/

#ifndef NIMBLE_SERVER_INCOMING_STEPS_H
#define NIMBLE_SERVER_INCOMING_STEPS_H

#include <nimble-steps/steps.h>
#include <stddef.h>
#include <stdint.h>

struct NimbleServerGame;
struct NimbleServerLocalParty;
struct NimbleServerLocalParties;
struct NimbleServerTransportConnection;
struct FldOutStream;
struct FldInStream;

int nimbleServerHandleIncomingSteps(struct NimbleServerGame* foundGame, struct FldInStream* inStream,
                                    struct NimbleServerTransportConnection* transportConnection,
                                    StepId* outClientWaitingForStepId);

#endif
