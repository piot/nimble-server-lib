/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <clog/clog.h>
#include <nimble-server/participant.h>
#include <nimble-server/participants.h>
#include "imprint/allocator.h"

int nbdParticipantsJoin(NbdParticipants* self, const NbdParticipantJoinInfo* joinInfo, size_t localParticipantCount,
                        NbdParticipant** results)
{
    if (self->participantCount + localParticipantCount > self->participantCapacity) {
        CLOG_WARN("couldn't join, session is full")
        return -2;
    }
    size_t joinIndex = 0;

    for (size_t i = 0; i < self->participantCapacity; ++i) {
        struct NbdParticipant* participant = &self->participants[i];
        if (!participant->isUsed) {
            const NbdParticipantJoinInfo* joiner = &joinInfo[joinIndex];
            participant->localIndex = joiner->localIndex;
            participant->isUsed = true;
            participant->id = ++self->lastUniqueId;
            results[joinIndex] = participant;
            joinIndex++;
            if (joinIndex == localParticipantCount) {
                break;
            }
        }
    }

    if (joinIndex != localParticipantCount) {
        return -2;
    }

    return 0;
}

void nbdParticipantsInit(NbdParticipants* self, ImprintAllocator* allocator, size_t maxCount)
{
    self->participantCapacity = maxCount;
    self->participants = IMPRINT_CALLOC_TYPE_COUNT(allocator, NbdParticipant, maxCount);
    self->participantCount = 0;
    self->lastUniqueId = 0;
}

void nbdParticipantsDestroy(NbdParticipants* self)
{
}
