/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/nimble-server-lib
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/
#ifndef NIMBLE_SERVER_PARTICIPANTS_H
#define NIMBLE_SERVER_PARTICIPANTS_H

#include <clog/clog.h>
#include <nimble-serialize/types.h>
#include <stdint.h>
#include <stdlib.h>
#include <nimble-server/circular_buffer.h>

struct NimbleServerParticipant;
struct ImprintAllocator;

/// All the participants that are within a game
typedef struct NimbleServerParticipants {
    struct NimbleServerParticipant* participants;
    size_t participantCapacity;
    size_t participantCount;
    NimbleServerCircularBuffer freeList;
    Clog log;
} NimbleServerParticipants;

typedef struct NimbleServerParticipantJoinInfo {
    uint8_t localIndex;
} NimbleServerParticipantJoinInfo;

void nimbleServerParticipantsInit(NimbleServerParticipants* self, struct ImprintAllocator* allocator, size_t maxCount,
                                  Clog* log);
int nimbleServerParticipantsJoin(NimbleServerParticipants* self, const NimbleSerializeJoinGameRequestPlayer* joinInfo,
                                 size_t localParticipantCount, struct NimbleServerParticipant** results);
void nimbleServerParticipantsDestroy(NimbleServerParticipants* self, NimbleSerializeParticipantId participantId);
int nimbleServerParticipantsPrepare(NimbleServerParticipants* self, NimbleSerializeParticipantId participantId,
                                    struct NimbleServerParticipant** createdParticipant);

struct NimbleServerParticipant* nimbleServerParticipantsFind(NimbleServerParticipants* self, NimbleSerializeParticipantId participantId);

#endif
