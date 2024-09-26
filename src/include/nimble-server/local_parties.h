/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/nimble-server-lib
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/

#ifndef NIMBLE_SERVER_PARTICIPANT_CONNECTIONS_H
#define NIMBLE_SERVER_PARTICIPANT_CONNECTIONS_H

#include <stdint.h>
#include <stdlib.h>

#include <clog/clog.h>
#include <nimble-server/participants.h>
#include <nimble-steps/steps.h>
#include <nimble-serialize/types.h>

struct NimbleServerLocalParty;

struct ImprintAllocator;
struct ImprintAllocatorWithFree;
struct NimbleServerTransportConnection;

typedef struct NimbleServerLocalParties {
    struct NimbleServerLocalParty* parties;
    size_t partiesCount;
    size_t capacityCount;
    struct ImprintAllocator* allocator;
    size_t maxLocalPartyParticipantCount;
    size_t maxSingleParticipantStepOctetCount;
    Clog log;
} NimbleServerLocalParties;

void nimbleServerLocalPartiesInit(NimbleServerLocalParties* self, size_t maxCount,
                                            struct ImprintAllocator* connectionAllocator,
                                            size_t maxNumberOfParticipantsForConnection,
                                            size_t maxSingleParticipantOctetCount, Clog log);
void nimbleServerLocalPartiesReset(NimbleServerLocalParties* self);
void nimbleServerLocalPartiesRemove(NimbleServerLocalParties* self,
                                              struct NimbleServerLocalParty* connection);
struct NimbleServerLocalParty*
nimbleServerLocalPartiesFindParty(NimbleServerLocalParties* self, uint8_t connectionIndex);
struct NimbleServerLocalParty*
nimbleServerLocalPartiesFindPartyForTransport(NimbleServerLocalParties* self,
                                                             uint32_t transportConnectionId);
struct NimbleServerLocalParty*
nimbleServerLocalPartiesFindPartyFromSecret(NimbleServerLocalParties* self,
                                                           NimbleSerializePartyAndSessionSecret partyAndSessionSecret);
struct NimbleServerLocalParty*
nimbleServerLocalPartiesFindParticipantPartyFromId(NimbleServerLocalParties* self,
                                                         NimbleSerializeLocalPartyId partyId);

int nimbleServerLocalPartiesCreate(NimbleServerLocalParties* self,
                                             NimbleServerParticipants* gameParticipants,
                                             struct NimbleServerTransportConnection* transportConnection,
                                             const NimbleSerializeJoinGameRequestPlayer* joinInfo,
                                             StepId latestAuthoritativeStepId, size_t localParticipantCount,
                                             struct NimbleServerLocalParty** outConnection);

int nimbleServerLocalPartiesPrepare(NimbleServerLocalParties* self, NimbleServerParticipants* gameParticipants,
                                    StepId latestAuthoritativeStepId, NimbleSerializeLocalPartyInfo partyInfo,
                                    struct NimbleServerLocalParty** outParty);
#endif
