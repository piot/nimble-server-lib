/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/nimble-server-lib
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/
#include <clog/clog.h>
#include <nimble-server/errors.h>
#include <nimble-server/participant.h>
#include <nimble-server/local_party.h>
#include <nimble-server/local_parties.h>
#include <nimble-server/transport_connection.h>
#include <secure-random/secure_random.h>

void nimbleServerLocalPartiesInit(NimbleServerLocalParties* self, size_t maxCount,
                                            ImprintAllocator* connectionAllocator,
                                            size_t maxLocalPartyParticipantCount,
                                            size_t maxSingleParticipantOctetCount, Clog log)
{
    self->partiesCount = 0;
    self->parties = IMPRINT_ALLOC_TYPE_COUNT(connectionAllocator, NimbleServerLocalParty, maxCount);
    self->capacityCount = maxCount;
    self->allocator = connectionAllocator;
    self->maxLocalPartyParticipantCount = maxLocalPartyParticipantCount;
    self->maxSingleParticipantStepOctetCount = maxSingleParticipantOctetCount;
    self->log = log;

    tc_mem_clear_type_n(self->parties, self->capacityCount);

    for (size_t i = 0; i < self->capacityCount; ++i) {
        Clog subLog;
        subLog.constantPrefix = "NimbleServerLocalParty";
        subLog.config = log.config;
        nimbleServerLocalPartyInit(&self->parties[i], 0, connectionAllocator, 0xffffffff,
                                   maxLocalPartyParticipantCount, maxSingleParticipantOctetCount,
                                              subLog);
    }
}

void nimbleServerLocalPartiesReset(NimbleServerLocalParties* self)
{
    for (size_t i = 0; i < self->capacityCount; ++i) {
        nimbleServerLocalPartyReset(&self->parties[i]);
    }
}

/// Finds the party using the internal index
/// @param self parties
/// @param connectionIndex connection index to find
/// @return return NULL if connection was not found
struct NimbleServerLocalParty*
nimbleServerLocalPartiesFindParty(NimbleServerLocalParties* self, NimbleSerializeParticipantPartyId connectionIndex)
{
    if (connectionIndex >= self->capacityCount) {
        CLOG_C_ERROR(&self->log, "Illegal connection index: %d", connectionIndex)
        // return 0;
    }

    return &self->parties[connectionIndex];
}

/// Finds the party for the specified connection id
/// @param self parties
/// @param transportConnectionId find party for the specified transport connection
/// @return pointer to a party, or NULL otherwise
struct NimbleServerLocalParty*
nimbleServerLocalPartiesFindPartyForTransport(NimbleServerLocalParties* self,
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

static NimbleServerLocalParty* findFreePartyPlaceButDoNotReserveItYet(NimbleServerLocalParties* self)
{
    for (size_t i = 0; i < self->capacityCount; ++i) {
        NimbleServerLocalParty* party = &self->parties[i];
        if (party->isUsed) {
            continue;
        }
        party->id = (NimbleSerializeParticipantPartyId) i;
        CLOG_C_DEBUG(&self->log,
                     "found free local party to use at #%zu. capacity before allocating: (%zu/%zu)", i,
                     self->partiesCount, self->capacityCount)

        return party;
    }

    CLOG_C_NOTICE(&self->log, "out of parties from the pre-allocated pool")

    return 0;
}

static void addParty(NimbleServerLocalParties* self,
                                     NimbleServerLocalParty* party,
                                     struct NimbleServerTransportConnection* transportConnection,
                                     StepId latestAuthoritativeStepId, NimbleServerParticipants* gameParticipants,
                                     struct NimbleServerParticipant* createdParticipants[16],
                                     size_t localParticipantCount)
{
    tc_snprintf(party->debugPrefix, sizeof(party->debugPrefix), "%s/%u",
                self->log.constantPrefix, party->id);
    party->log.constantPrefix = party->debugPrefix;
    party->log.config = self->log.config;

    nimbleServerLocalPartyInit(party, transportConnection, self->allocator,
                                          latestAuthoritativeStepId, self->maxLocalPartyParticipantCount,
                                          self->maxSingleParticipantStepOctetCount, party->log);
    self->partiesCount++;
    party->isUsed = true;

    party->participantReferences.gameParticipants = gameParticipants;
    for (size_t participantIndex = 0; participantIndex < localParticipantCount; ++participantIndex) {
        party->participantReferences
            .participantReferences[participantIndex] = createdParticipants[participantIndex];
    }

    party->participantReferences.participantReferenceCount = localParticipantCount;
    //party->partySecret.value = secureRandomUInt64();

    CLOG_C_DEBUG(&party->log, "party is ready. All participants have joined")
}

int nimbleServerLocalPartiesPrepare(NimbleServerLocalParties* self,
                                              NimbleServerParticipants* gameParticipants,
                                              StepId latestAuthoritativeStepId,
                                              NimbleSerializeParticipantId participantId,
                                              NimbleServerLocalParty** outConnection)
{
    NimbleServerLocalParty* party = findFreePartyPlaceButDoNotReserveItYet(self);
    if (!party) {
        CLOG_C_NOTICE(&self->log, "could not join, because out of party memory")
        return NimbleServerErrOutOfParticipantMemory;
    }

    struct NimbleServerParticipant* createdParticipants[16];

    int errorCode = nimbleServerParticipantsPrepare(gameParticipants, participantId, createdParticipants);
    if (errorCode < 0) {
        *outConnection = 0;
        return errorCode;
    }

    addParty(self, party, 0, latestAuthoritativeStepId, gameParticipants,
                             createdParticipants, 1);

    party->isUsed = true;
    party->state = NimbleServerLocalPartyStateWaitingForReJoin;
    party->waitingForReconnectTimer = 0;

    *outConnection = party;

    return 0;
}

/// Creates a party for the specified room.
/// @note In this version there must be an existing game in the room.
/// @param self parties
/// @param gameParticipants participants collection
/// @param transportConnection the transport connection to create the party for
/// @param joinInfo information about the joining participants
/// @param latestAuthoritativeStepId latest known authoritative stepID
/// @param localParticipantCount local participant count
/// @param[out] outConnection the created connection
/// @return negative on error
int nimbleServerLocalPartiesCreate(NimbleServerLocalParties* self,
                                             NimbleServerParticipants* gameParticipants,
                                             struct NimbleServerTransportConnection* transportConnection,
                                             const NimbleSerializeJoinGameRequestPlayer* joinInfo,
                                             StepId latestAuthoritativeStepId, size_t localParticipantCount,
                                             NimbleServerLocalParty** outConnection)
{
    NimbleServerLocalParty* party = findFreePartyPlaceButDoNotReserveItYet(self);
    if (!party) {
        CLOG_C_NOTICE(&self->log, "could not join, because out of party memory")
        return NimbleServerErrOutOfParticipantMemory;
    }

    struct NimbleServerParticipant* createdParticipants[16];

    int errorCode = nimbleServerParticipantsJoin(gameParticipants, joinInfo, localParticipantCount,
                                                 createdParticipants);
    if (errorCode < 0) {
        *outConnection = 0;
        return errorCode;
    }

    addParty(self, party, transportConnection, latestAuthoritativeStepId,
                             gameParticipants, createdParticipants, localParticipantCount);

    *outConnection = party;

    return 0;
}


/// Remove party from parties collection
/// @param self parties
/// @param party to remove from collection
void nimbleServerLocalPartiesRemove(NimbleServerLocalParties* self,
                                              struct NimbleServerLocalParty* party)
{
    (void) self;

    CLOG_ASSERT(party->isUsed, "party must be in use to be removed")

    CLOG_C_DEBUG(&self->log, "removing party %u ", party->id)

    CLOG_ASSERT(self->partiesCount != 0, "parties count is wrong %zu", self->partiesCount)

    self->partiesCount--;

    CLOG_C_NOTICE(&self->log, "now has %zu parties left", self->partiesCount)

    nimbleServerLocalPartyReset(party);
}
