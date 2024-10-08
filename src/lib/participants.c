/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/nimble-server-lib
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/

#include <clog/clog.h>
#include <imprint/allocator.h>
#include <nimble-server/errors.h>
#include <nimble-server/participant.h>
#include <nimble-server/participants.h>

/// Initializes and allocates memory the participant collection
/// @param self participants collection
/// @param allocator allocator to pre-alloc the collection
/// @param maxCount maximum number of participants to pre-alloc
void nimbleServerParticipantsInit(NimbleServerParticipants* self, ImprintAllocator* allocator, size_t maxCount,
                                  size_t maxStepOctetSize, Clog* log)
{
    CLOG_ASSERT(maxCount > 0, "must allocate at least one participant")
    self->log = *log;
    tc_snprintf(self->debugPrefix, sizeof(self->debugPrefix), "%s/participants", self->log.constantPrefix);
    self->log.constantPrefix = self->debugPrefix;

    self->participantCapacity = maxCount;
    self->participants = IMPRINT_CALLOC_TYPE_COUNT(allocator, NimbleServerParticipant, maxCount);
    self->participantCount = 0;

    nimbleServerCircularBufferInit(&self->freeList);

    CLOG_ASSERT(maxCount < NIMBLE_SERVER_CIRCULAR_BUFFER_SIZE,
                "maxCount must be less than NIMBLE_SERVER_CIRCULAR_BUFFER_SIZE")
    for (size_t i = 0; i < maxCount; ++i) {
        nimbleServerCircularBufferWrite(&self->freeList, (uint8_t) i);
    }

    CLOG_C_DEBUG(&self->log, "allocating %zu participants as capacity", maxCount)
    for (uint8_t i = 0; i < maxCount; ++i) {
        NimbleServerParticipant* participant = &self->participants[i];

        CLOG_C_DEBUG(&self->log, "preparing participant %hhu", i)
        NimbleServerParticipantSetup setup = {
            .id = i,
            .maxStepOctetSizeForOneParticipant = maxStepOctetSize,
            .connectionAllocator = allocator,
        };

        tc_snprintf(participant->debugPrefix, sizeof(participant->debugPrefix), "%s/%u", self->log.constantPrefix,
                    setup.id);
        setup.log.constantPrefix = participant->debugPrefix;
        setup.log.config = self->log.config;

        nimbleServerParticipantInit(participant, setup);
    }

    CLOG_ASSERT(self->participants[0].isUsed == false, "CALLOC did not work")
}

/// Marks the participant as not used anymore
/// @param self participants collection
/// @param participantId the participant to mark as not used anymore (destroyed).
void nimbleServerParticipantsDestroy(NimbleServerParticipants* self, NimbleSerializeParticipantId participantId)
{
    CLOG_C_DEBUG(&self->log, "destroying participant %hhu", participantId)

    CLOG_ASSERT(self->participantCount > 0, "participant count is wrong when removing game participants")
    self->participantCount--;

    if (participantId >= self->participantCapacity) {
        CLOG_C_ERROR(&self->log, "illegal participant id %hhu capacity: %zu", participantId, self->participantCapacity)
    }
    struct NimbleServerParticipant* participant = &self->participants[participantId];

    if (!participant->isUsed) {
        CLOG_ERROR("internal error, tried to free an unused participant")
    }

    nimbleServerCircularBufferWrite(&self->freeList, participantId);
    nimbleServerParticipantDestroy(participant);
}

int nimbleServerParticipantsPrepare(NimbleServerParticipants* self, NimbleSerializeParticipantId participantId,
                                    struct NimbleServerLocalParty* party, StepId currentAuthoritativeStepId,
                                    struct NimbleServerParticipant** outConnection)
{
    if (self->participantCount + 1 > self->participantCapacity) {
        CLOG_C_WARN(&self->log, "couldn't join, game participants are at full capacity: %zu", self->participantCapacity)
        return NimbleServerErrSessionFull;
    }

    CLOG_C_DEBUG(&self->log, "the participant can join, the game participant pool is at %zu/%zu",
                 self->participantCount, self->participantCapacity)

    if (participantId >= self->participantCapacity) {
        CLOG_C_ERROR(&self->log, "illegal participant id %hhu capacity: %zu", participantId, self->participantCapacity)
    }
    struct NimbleServerParticipant* participant = &self->participants[participantId];

    if (participant->isUsed) {
        CLOG_SOFT_ERROR("could not prepare for host migration, participant already used")
        return -2;
    }
    nimbleServerParticipantReInit(participant, party, currentAuthoritativeStepId);

    participant->localIndex = 0;
    participant->isUsed = true;
    participant->state = NimbleServerParticipantStateWaitingForRejoin;
    participant->id = participantId;

    CLOG_C_DEBUG(&self->log, "allocating participant with game unique id: %hhu", participant->id)

    self->participantCount++;
    *outConnection = participant;

    return 0;
}

NimbleServerParticipant* nimbleServerParticipantsFind(NimbleServerParticipants* self,
                                                      NimbleSerializeParticipantId participantId)
{
    if (participantId >= self->participantCapacity) {
        return 0;
    }

    return &self->participants[participantId];
}

/// Creates a new participant using the join information.
/// @param self participants collection
/// @param localPlayers information about joining local players
/// @param localParticipantCount the number of participants in joinInfo
/// @param[out] results the resulting participant
/// @return negative on error
int nimbleServerParticipantsJoin(NimbleServerParticipants* self,
                                 const NimbleSerializeJoinGameRequestPlayer* localPlayers, size_t localParticipantCount,
                                 struct NimbleServerLocalParty* party, StepId expectedStepId,
                                 NimbleServerParticipant** results)
{
    CLOG_ASSERT(localParticipantCount == 1, "in this version of nimble only one local participant is supported")

    if (self->participantCount + localParticipantCount > self->participantCapacity) {
        CLOG_C_WARN(&self->log, "couldn't join, game participants are at full capacity: %zu", self->participantCapacity)
        return NimbleServerErrSessionFull;
    }

    CLOG_C_DEBUG(&self->log, "joining %zu participant(s), the game participant pool is at %zu/%zu",
                 localParticipantCount, self->participantCount, self->participantCapacity)

    size_t joinIndex = 0;
    NimbleSerializeParticipantId participantId = nimbleServerCircularBufferRead(&self->freeList);

    if (participantId >= self->participantCapacity) {
        CLOG_C_ERROR(&self->log, "illegal participant id %hhu capacity: %zu", participantId, self->participantCapacity)
    }
    struct NimbleServerParticipant* participant = &self->participants[participantId];
    if (participant->isUsed) {
        CLOG_ERROR("internal error, participant is already used, even if the index came from the free list")
    }

    nimbleServerParticipantReInit(participant, party, expectedStepId);

    const NimbleSerializeJoinGameRequestPlayer* localPlayer = &localPlayers[joinIndex];
    participant->localIndex = localPlayer->localIndex;
    participant->isUsed = true;
    participant->state = NimbleServerParticipantStateJustJoined;
    participant->id = participantId;

    CLOG_C_DEBUG(&self->log, "allocating participant %zu with unique id: %hhu", joinIndex, participant->id)

    results[joinIndex++] = participant;
    self->participantCount++;

    if (joinIndex != localParticipantCount) {
        CLOG_ERROR("internal error %zu vs %zu", joinIndex, localParticipantCount)
        // return -2;
    }

    return 0;
}
