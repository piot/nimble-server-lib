/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/nimble-server-lib
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/
#include <clog/clog.h>
#include <nimble-server/errors.h>
#include <nimble-server/local_parties.h>
#include <nimble-server/local_party.h>
#include <nimble-server/participant.h>
#include <nimble-server/transport_connection.h>
#include <secure-random/secure_random.h>

/// Allocates memory for the local parties collection
/// @param self local parties collection
/// @param maxCount capacity for the collection
/// @param allocator the allocator to use to reserve memory for the number of parties.
/// @param maxLocalPartyParticipantCount maximum number of participants in a party.
/// @param maxSingleParticipantOctetCount the maximum number of octets for one step for a participant.
/// @param log logging
void nimbleServerLocalPartiesInit(NimbleServerLocalParties* self, size_t maxCount, ImprintAllocator* allocator,
                                  size_t maxLocalPartyParticipantCount, size_t maxSingleParticipantOctetCount, Clog log)
{
    self->partiesCount = 0;
    self->parties = IMPRINT_ALLOC_TYPE_COUNT(allocator, NimbleServerLocalParty, maxCount);
    self->capacityCount = maxCount;
    self->allocator = allocator;
    self->maxLocalPartyParticipantCount = maxLocalPartyParticipantCount;
    self->maxSingleParticipantStepOctetCount = maxSingleParticipantOctetCount;
    self->log = log;

    tc_mem_clear_type_n(self->parties, self->capacityCount);

    for (size_t i = 0; i < self->capacityCount; ++i) {
        NimbleServerLocalParty* party = &self->parties[i];
        uint8_t id = (uint8_t) i;

        Clog subLog;
        tc_snprintf(party->debugPrefix, sizeof(party->debugPrefix), "%s/party/%u", self->log.constantPrefix, id);
        subLog.constantPrefix = party->debugPrefix;
        subLog.config = log.config;

        nimbleServerLocalPartyInit(party, id, subLog);
    }
}

/// Resets the memory of the party collection
/// @param self party collection
void nimbleServerLocalPartiesReset(NimbleServerLocalParties* self)
{
    for (size_t i = 0; i < self->capacityCount; ++i) {
        nimbleServerLocalPartyReset(&self->parties[i]);
    }
}

/// Finds the party using the internal index
/// @param self parties
/// @param partyId party to find
/// @return return NULL if party was not found
struct NimbleServerLocalParty* nimbleServerLocalPartiesFindParty(NimbleServerLocalParties* self,
                                                                 NimbleSerializeLocalPartyId partyId)
{
    if (partyId >= self->capacityCount) {
        CLOG_C_ERROR(&self->log, "Illegal party id: %d", partyId)
        // return 0;
    }

    return &self->parties[partyId];
}

/// Finds the party for with the specified transport id
/// @param self parties
/// @param transportConnectionId find party for the specified transport connection
/// @return pointer to a party, or NULL otherwise
struct NimbleServerLocalParty* nimbleServerLocalPartiesFindPartyForTransport(NimbleServerLocalParties* self,
                                                                             uint32_t transportConnectionId)
{
    for (size_t i = 0; i < self->partiesCount; ++i) {
        if (self->parties[i].isUsed && self->parties[i].transportConnection &&
            self->parties[i].transportConnection->transportConnectionId == transportConnectionId) {
            return &self->parties[i];
        }
    }

    return 0;
}

/// Looks up a party that is not used
/// @param self party collection
/// @return zero if out of capacity, or a pointer to the reserved party.
static NimbleServerLocalParty* findFreePartyPlaceButDoNotReserveItYet(NimbleServerLocalParties* self)
{
    for (size_t i = 0; i < self->capacityCount; ++i) {
        NimbleServerLocalParty* party = &self->parties[i];
        if (party->isUsed) {
            continue;
        }
        party->id = (NimbleSerializeLocalPartyId) i;
        CLOG_C_DEBUG(&self->log, "found free local party to use at #%zu. capacity before allocating: (%zu/%zu)", i,
                     self->partiesCount, self->capacityCount)

        return party;
    }

    CLOG_C_NOTICE(&self->log, "out of parties from the pre-allocated pool")

    return 0;
}

/// Adds a party for the specified transport connection and created participants.
/// @param self party collection
/// @param party the party to assign the participants to
/// @param transportConnection the transport connection where the party happens.
/// @param createdParticipants the previously created participants.
/// @param localParticipantCount how many participants is present in the createdParticipants.
static void addParty(NimbleServerLocalParties* self, NimbleServerLocalParty* party,
                     struct NimbleServerTransportConnection* transportConnection,
                     struct NimbleServerParticipant* createdParticipants[16], size_t localParticipantCount)
{
    nimbleServerLocalPartyReInit(party, transportConnection);
    self->partiesCount++;
    party->isUsed = true;

    for (size_t participantIndex = 0; participantIndex < localParticipantCount; ++participantIndex) {
        party->participantReferences.participantReferences[participantIndex] = createdParticipants[participantIndex];
    }

    party->participantReferences.participantReferenceCount = localParticipantCount;
    // party->partySecret.value = secureRandomUInt64();

    CLOG_C_DEBUG(&party->log, "party is ready. All participants have joined")
}

/// Used in host migration to prepare a party and the participants in that party.
/// @param self party collection
/// @param gameParticipants the game participants collection to use to propare the participants
/// @param latestAuthoritativeStepId the tick id to start host migration from.
/// @param partyInfo information about the local party
/// @param outParty the created and prepared party
/// @return negative on error.
int nimbleServerLocalPartiesPrepare(NimbleServerLocalParties* self, NimbleServerParticipants* gameParticipants,
                                    StepId latestAuthoritativeStepId, NimbleSerializeLocalPartyInfo partyInfo,
                                    NimbleServerLocalParty** outParty)
{
    NimbleServerLocalParty* party = findFreePartyPlaceButDoNotReserveItYet(self);
    if (!party) {
        CLOG_C_NOTICE(&self->log, "could not join, because out of party memory")
        return NimbleServerErrOutOfParticipantMemory;
    }

    struct NimbleServerParticipant* createdParticipants[16];

    for (size_t participantIndex = 0; participantIndex < partyInfo.participantCount; ++participantIndex) {
        int errorCode = nimbleServerParticipantsPrepare(gameParticipants, partyInfo.participantIds[participantIndex],
                                                        party, latestAuthoritativeStepId,
                                                        &createdParticipants[participantIndex]);
        if (errorCode < 0) {
            *outParty = 0;
            return errorCode;
        }
    }

    addParty(self, party, 0, createdParticipants, 1);

    party->isUsed = true;
    party->state = NimbleServerLocalPartyStateWaitingForReJoin;
    party->waitingForReconnectTimer = 0;

    *outParty = party;

    return 0;
}

/// Creates a party for the specified participants and transport connection.
/// @param self parties collection
/// @param gameParticipants participants collection
/// @param transportConnection the transport connection to create the party for
/// @param joinInfo information about the joining participants
/// @param latestAuthoritativeStepId latest known authoritative stepID
/// @param localParticipantCount local participant count
/// @param[out] outParty the created party
/// @return negative on error
int nimbleServerLocalPartiesCreate(NimbleServerLocalParties* self, NimbleServerParticipants* gameParticipants,
                                   struct NimbleServerTransportConnection* transportConnection,
                                   const NimbleSerializeJoinGameRequestPlayer* joinInfo,
                                   StepId latestAuthoritativeStepId, size_t localParticipantCount,
                                   NimbleServerLocalParty** outParty)
{
    NimbleServerLocalParty* party = findFreePartyPlaceButDoNotReserveItYet(self);
    if (!party) {
        CLOG_C_NOTICE(&self->log, "could not join, because out of party memory")
        return NimbleServerErrOutOfParticipantMemory;
    }

    struct NimbleServerParticipant* createdParticipants[16];

    int errorCode = nimbleServerParticipantsJoin(gameParticipants, joinInfo, localParticipantCount, party,
                                                 latestAuthoritativeStepId, createdParticipants);
    if (errorCode < 0) {
        *outParty = 0;
        return errorCode;
    }

    addParty(self, party, transportConnection, createdParticipants, localParticipantCount);

    *outParty = party;

    return 0;
}

/// Remove party from parties collection (mark it as not used).
/// @param self parties
/// @param party to remove from collection
void nimbleServerLocalPartiesRemove(NimbleServerLocalParties* self, struct NimbleServerLocalParty* party)
{
    (void) self;

    CLOG_ASSERT(party->isUsed, "party must be in use to be removed")

    CLOG_C_DEBUG(&self->log, "removing party %u ", party->id)

    CLOG_ASSERT(self->partiesCount != 0, "parties count is wrong %zu", self->partiesCount)

    self->partiesCount--;

    CLOG_C_NOTICE(&self->log, "now has %zu parties left", self->partiesCount)

    nimbleServerLocalPartyReset(party);
}
