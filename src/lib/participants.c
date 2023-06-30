/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <clog/clog.h>
#include <imprint/allocator.h>
#include <nimble-server/participant.h>
#include <nimble-server/participants.h>

/// Creates a new participant using the join information.
/// @param self participants collection
/// @param joinInfo information about joining participants
/// @param localParticipantCount the number of participants in joinInfo
/// @param[out] results the resulting participant
/// @return negative on error
int nimbleServerParticipantsJoin(NimbleServerParticipants* self, const NimbleServerParticipantJoinInfo* joinInfo, size_t localParticipantCount,
                        NimbleServerParticipant** results)
{
    if (self->participantCount + localParticipantCount > self->participantCapacity) {
        CLOG_WARN("couldn't join, session is full")
        return -2;
    }
    size_t joinIndex = 0;

    for (size_t i = 0; i < self->participantCapacity; ++i) {
        struct NimbleServerParticipant* participant = &self->participants[i];
        if (!participant->isUsed) {
            const NimbleServerParticipantJoinInfo* joiner = &joinInfo[joinIndex];
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

/// Initializes and allocates memory the participant collection
/// @param self participants collection
/// @param allocator allocator to pre-alloc the collection
/// @param maxCount maximum number of participants to pre-alloc
void nimbleServerParticipantsInit(NimbleServerParticipants* self, ImprintAllocator* allocator, size_t maxCount)
{
    self->participantCapacity = maxCount;
    self->participants = IMPRINT_CALLOC_TYPE_COUNT(allocator, NimbleServerParticipant, maxCount);
    self->participantCount = 0;
    self->lastUniqueId = 0;
}
