/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#ifndef NIMBLE_SERVER_REQ_PARTICIPANTS_H
#define NIMBLE_SERVER_REQ_PARTICIPANTS_H

#include <nimble-steps/steps.h>

struct FldInStream;
struct NbdParticipantConnection;
struct NbdParticipantConnections;

int nbdReadAndJoinParticipants(struct NbdParticipantConnections* connections, NbdParticipants* gameParticipants,
                               size_t transportConnectionId, struct FldInStream* inStream,
                               StepId latestAuthoritativeStepId, struct NbdParticipantConnection** createdConnection);

#endif
