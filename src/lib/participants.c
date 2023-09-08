/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <clog/clog.h>
#include <imprint/allocator.h>
#include <nimble-server/participant.h>
#include <nimble-server/participants.h>
#include <nimble-server/errors.h>

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
        CLOG_C_WARN(&self->log, "couldn't join, game participants are at full capacity: %zu", self->participantCapacity)
        return NimbleServerErrSessionFull;
    }

    CLOG_C_DEBUG(&self->log, "the %zu participant(s) can join, the game participant pool is at %zu/%zu", localParticipantCount, self->participantCount, self->participantCapacity)

    size_t joinIndex = 0;

    for (size_t i = 0; i < self->participantCapacity; ++i) {
        struct NimbleServerParticipant* participant = &self->participants[i];
        if (!participant->isUsed) {
            const NimbleServerParticipantJoinInfo* joiner = &joinInfo[joinIndex];
            participant->localIndex = joiner->localIndex;
            participant->isUsed = true;
            participant->id = ++self->lastUniqueId;

            CLOG_C_DEBUG(&self->log, "allocating participant %zu at index: %zu with unique id: %zu", joinIndex, i, participant->id)

            results[joinIndex++] = participant;
            self->participantCount++;

            if (joinIndex == localParticipantCount) {
                 CLOG_C_DEBUG(&self->log, "all are allocated, so breaking out of loop. i:%zu, joinIndex:%zu, count:%zu", i, joinIndex, localParticipantCount)
                break;
            }
        } else {
            CLOG_C_DEBUG(&self->log, "participant at %zu is already used. allocated %zu count so far", i, joinIndex)
        }
    }

    if (joinIndex != localParticipantCount) {
        CLOG_ERROR("internal error %zu vs %zu", joinIndex, localParticipantCount)
        //return -2;
    }

    return 0;
}

/// Initializes and allocates memory the participant collection
/// @param self participants collection
/// @param allocator allocator to pre-alloc the collection
/// @param maxCount maximum number of participants to pre-alloc
void nimbleServerParticipantsInit(NimbleServerParticipants* self, ImprintAllocator* allocator, size_t maxCount, Clog* log)
{
    CLOG_ASSERT(maxCount > 0, "must allocate at least one participant")
    self->log = *log;
    self->log.constantPrefix = "server/participants";
    self->participantCapacity = maxCount;
    self->participants = IMPRINT_CALLOC_TYPE_COUNT(allocator, NimbleServerParticipant, maxCount);
    self->participantCount = 0;
    self->lastUniqueId = 0;

    CLOG_C_DEBUG(&self->log, "allocating %zu server participants as capacity", maxCount)
    CLOG_ASSERT(self->participants[0].isUsed == false, "CALLOC did not work")
}
