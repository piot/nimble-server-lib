/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#ifndef NIMBLE_SERVER_PARTICIPANTS_H
#define NIMBLE_SERVER_PARTICIPANTS_H

#include <stdint.h>
#include <stdlib.h>

struct NimbleServerParticipant;
struct ImprintAllocator;

/// All the participants that are within a game
typedef struct NimbleServerParticipants {
    struct NimbleServerParticipant* participants;
    size_t participantCapacity;
    size_t participantCount;
    uint8_t lastUniqueId;
} NimbleServerParticipants;

typedef struct NimbleServerParticipantJoinInfo {
    uint8_t localIndex;
} NimbleServerParticipantJoinInfo;

void nbdParticipantsInit(NimbleServerParticipants* self, struct ImprintAllocator* allocator, size_t maxCount);
int nbdParticipantsJoin(NimbleServerParticipants* self, const NimbleServerParticipantJoinInfo* joinInfo, size_t localParticipantCount,
                        struct NimbleServerParticipant** results);

#endif
