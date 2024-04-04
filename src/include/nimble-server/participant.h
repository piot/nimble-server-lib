/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/nimble-server-lib
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/
#ifndef NIMBLE_SERVER_PARTICIPANT_H
#define NIMBLE_SERVER_PARTICIPANT_H

#include <stdbool.h>
#include <stdlib.h>

typedef struct NimbleServerParticipant {
    size_t localIndex;
    uint8_t id;
    bool isUsed;
} NimbleServerParticipant;

#endif
