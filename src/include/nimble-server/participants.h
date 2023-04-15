/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#ifndef NIMBLE_SERVER_PARTICIPANTS_H
#define NIMBLE_SERVER_PARTICIPANTS_H

#include <stdint.h>
#include <stdlib.h>

struct NbdParticipant;
struct ImprintAllocator;

/// All the participants that are within a game
typedef struct NbdParticipants {
    struct NbdParticipant* participants;
    size_t participantCapacity;
    size_t participantCount;
    uint8_t lastUniqueId;
} NbdParticipants;

typedef struct NbdParticipantJoinInfo {
    uint8_t localIndex;
} NbdParticipantJoinInfo;

void nbdParticipantsInit(NbdParticipants* self, struct ImprintAllocator* allocator, size_t maxCount);
void nbdParticipantsDestroy(NbdParticipants* self);
int nbdParticipantsJoin(NbdParticipants* self, const NbdParticipantJoinInfo* joinInfo, size_t localParticipantCount,
                        struct NbdParticipant** results);

#endif
