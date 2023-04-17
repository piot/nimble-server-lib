/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#ifndef NIMBLE_SERVER_REQ_STEP_H
#define NIMBLE_SERVER_REQ_STEP_H

#include <stddef.h>
#include <stdint.h>

struct NbdGame;
struct NbdParticipantConnection;
struct NbdParticipantConnections;
struct FldOutStream;
struct FldInStream;

int nbdReqGameStep(struct NbdGame* game, struct NbdParticipantConnection* foundConnection,  struct NbdParticipantConnections * connections, struct FldInStream* inStream, struct FldOutStream* response);

#endif
