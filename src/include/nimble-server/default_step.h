/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#ifndef NIMBLE_SERVER_DEFAULT_STEP_H
#define NIMBLE_SERVER_DEFAULT_STEP_H

#include <stddef.h>
#include <stdint.h>

struct NbdParticipantConnection;

int nbdInsertDefaultSteps(struct NbdParticipantConnection* foundParticipantConnection, size_t dropped);
int nbdCreateDefaultStep(struct NbdParticipantConnection* connection, uint8_t* defaultStepBuffer, size_t maxCount);

#endif
