/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#ifndef NIMBLE_SERVER_AUTHORITATIVE_STEPS_H
#define NIMBLE_SERVER_AUTHORITATIVE_STEPS_H

#include <stats/stats_per_second.h>
#include <stddef.h>
#include <stdint.h>

struct NimbleServerGame;
struct NimbleServerParticipantConnections;

int nimbleServerComposeAuthoritativeSteps(struct NimbleServerGame* game,
                                          struct NimbleServerParticipantConnections* connections);

#endif
