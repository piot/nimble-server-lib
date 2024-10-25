/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/nimble-server-lib
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/

#include "authoritative_steps.h"
#include "incoming_predicted_steps.h"
#include "nimble-server/server.h"
#include <flood/in_stream.h>
#include <nimble-server/delayed_quality.h>
#include <nimble-server/errors.h>
#include <nimble-server/local_party.h>
#include <nimble-server/participant.h>
#include <nimble-steps-serialize/in_serialize.h>
#include <nimble-steps-serialize/out_serialize.h>

/// Initializes a party.
/// @param self the party
/// @param id the id for the party
/// @param log the log to use for logging
/// Need to create Participants to the game before associating them to the connection.
void nimbleServerLocalPartyInit(NimbleServerLocalParty* self, NimbleSerializeLocalPartyId id, Clog log)
{
    self->log = log;
    CLOG_C_DEBUG(&self->log, "initialize local party")

    self->id = id;
    self->participantReferences.participantReferenceCount = 0;
    self->waitingForReconnectMaxTimer = 62 * 20;
    self->isUsed = false;
    self->warningCount = 0;

    self->quality.log.config = log.config;
    tc_snprintf(self->quality.debugPrefix, sizeof(self->quality.debugPrefix), "%s/quality", self->log.constantPrefix);
    self->quality.log.constantPrefix = self->quality.debugPrefix;
    nimbleServerConnectionQualityInit(&self->quality, self->quality.log);
    nimbleServerConnectionQualityDelayedInit(&self->delayedQuality, self->quality.log);

    nimbleServerLocalPartyReInit(self, 0);
}

/// Reinitialize the party
/// @param self party
/// @param transportConnection the underlying transport connection
void nimbleServerLocalPartyReInit(NimbleServerLocalParty* self, NimbleServerTransportConnection* transportConnection)
{
    self->state = NimbleServerLocalPartyStateNormal;
    nimbleServerConnectionQualityReInit(&self->quality);
    nimbleServerConnectionQualityDelayedReset(&self->delayedQuality);
    statsIntInit(&self->incomingStepCountInBufferStats, 60);
    // Expect that the client will add steps for the next authoritative step
    self->transportConnection = transportConnection;
    self->waitingForReconnectTimer = 0;
    self->warningCount = 0;
    self->warningAboutZeroAddedSteps = 0;
}

/// Resets and reuses the memory of a party
/// @param self party
void nimbleServerLocalPartyReset(NimbleServerLocalParty* self)
{
    self->isUsed = false;
    self->participantReferences.participantReferenceCount = 0;
    self->waitingForReconnectTimer = 0;
    self->warningCount = 0;
    self->warningAboutZeroAddedSteps = 0;
    nimbleServerConnectionQualityReset(&self->quality);
    nimbleServerConnectionQualityDelayedReset(&self->delayedQuality);
}

/// Facilitates the rejoining process of a party using a new transport connection.
/// @param self Pointer to an instance of NimbleServerLocalParty.
/// @param transportConnection The new transport connection through which the party is rejoining.
void nimbleServerLocalPartyRejoin(NimbleServerLocalParty* self, NimbleServerTransportConnection* transportConnection)
{
    CLOG_C_DEBUG(&self->log, "rejoined from transport connection %hhu", transportConnection->transportConnectionId)
    nimbleServerLocalPartyReInit(self, transportConnection);
}

/// Sets the party in dissolved state.
/// @param self Pointer to an instance of NimbleServerLocalParty.
void nimbleServerLocalPartyDestroy(NimbleServerLocalParty* self)
{
    CLOG_C_DEBUG(&self->log, "dissolved the party")
    self->state = NimbleServerLocalPartyStateDissolved;
}

/// Sets the party in a waiting for reconnect state.
/// @param self Pointer to an instance of NimbleServerLocalParty.
static void setToWaitingForReJoin(NimbleServerLocalParty* self)
{
    CLOG_C_DEBUG(&self->log, "setting state to: waiting for rejoin")
    self->state = NimbleServerLocalPartyStateWaitingForReJoin;
    self->waitingForReconnectTimer = 0;

    for (size_t i = 0; i < self->participantReferences.participantReferenceCount; ++i) {
        NimbleServerParticipant* participant = self->participantReferences.participantReferences[i];
        participant->state = NimbleServerParticipantStateWaitingForRejoin;
    }
}

/// Monitors the waiting period for a reconnect attempt and determines if it should continue waiting or give up.
/// @param self Pointer to an instance of NimbleServerLocalParty
/// @return Returns true if the waiting period has not yet exceeded the maximum allowed time, indicating that it should
/// continue waiting for a connection to rejoin the party. Returns false to suggest that the waiting period has expired,
/// and the party should be considered for dissolving.
static bool tickWaitingForReconnect(NimbleServerLocalParty* self)
{
    self->waitingForReconnectTimer++;
    if (self->waitingForReconnectTimer < self->waitingForReconnectMaxTimer) {
        return true;
    }

    CLOG_C_DEBUG(&self->log, "gave up on reconnect, waited %zu ticks. recommending the party to be dissolved",
                 self->waitingForReconnectTimer)

    return false;
}

/// Checks the connection quality of a server party and updates its state accordingly.
/// It decides whether the connection is still viable or if the participant should be moved to a waiting for reconnect
/// state based on the quality assessment.
/// @param self Pointer to an instance of NimbleServerLocalParty.
static void tickNormal(NimbleServerLocalParty* self)
{
    bool shouldKeep = nimbleServerConnectionQualityDelayedTick(&self->delayedQuality, &self->quality);

    if (!shouldKeep && self->state != NimbleServerLocalPartyStateWaitingForReJoin) {
        CLOG_C_DEBUG(&self->log,
                     "connection quality recommended waiting for rejoin, so setting it to waiting for rejoin")
        setToWaitingForReJoin(self);
    }
}

/// Updates a party
/// @param self party
/// @return false if connection should be disconnected, true otherwise
bool nimbleServerLocalPartyTick(NimbleServerLocalParty* self)
{
    switch (self->state) {
        case NimbleServerLocalPartyStateNormal:
            tickNormal(self);
            break;
        case NimbleServerLocalPartyStateWaitingForReJoin:
            return tickWaitingForReconnect(self);
        case NimbleServerLocalPartyStateDissolved:
            break;
    }

    return true;
}

/// Checks if a participantId is in the party
/// @param self party to check
/// @param participantId participantId to check for
/// @return true if found, false otherwise
bool nimbleServerLocalPartyHasParticipantId(const NimbleServerLocalParty* self, uint8_t participantId)
{
    for (size_t i = 0; i < self->participantReferences.participantReferenceCount; ++i) {
        const NimbleServerParticipant* participant = self->participantReferences.participantReferences[i];
        if (participant->id == participantId) {
            return true;
        }
    }

    return false;
}

int nimbleServerLocalPartyDeserializePredictedSteps(NimbleServerLocalParty* self, FldInStream* inStream)
{
    uint32_t lowestCommonStepId;
    fldInStreamReadUInt32(inStream, &lowestCommonStepId);

    CLOG_C_VERBOSE(&self->log, "lowest tickId: %08X", lowestCommonStepId)

    uint8_t participantCount;
    fldInStreamReadUInt8(inStream, &participantCount);
    CLOG_C_VERBOSE(&self->log, "participant count %hhu", participantCount)

    for (size_t participantIterator = 0; participantIterator < participantCount; ++participantIterator) {
        uint8_t participantId;

        fldInStreamReadUInt8(inStream, &participantId);
        if (participantId >= self->participantReferences.participantReferenceCount) {
            CLOG_C_SOFT_ERROR(&self->log, "illegal participant index %hhu", participantCount)
            return -4;
        }

        uint8_t deltaTicksFromCommonStepId;
        fldInStreamReadUInt8(inStream, &deltaTicksFromCommonStepId);

        uint32_t firstTickIdInArray = lowestCommonStepId + deltaTicksFromCommonStepId;
        CLOG_C_VERBOSE(&self->log, "first tickId in array: %08X (delta %hhu)", firstTickIdInArray,
                       deltaTicksFromCommonStepId)

        NimbleServerParticipant* participant = nimbleParticipantReferencesFind(&self->participantReferences,
                                                                               participantId);
        if (participant == 0) {
            CLOG_C_SOFT_ERROR(&self->log, "tried to insert participant not in the party")
            return -2;
        }

        uint8_t stepsThatFollow;
        fldInStreamReadUInt8(inStream, &stepsThatFollow);

        CLOG_EXECUTE(StepId lastStepId = (StepId) (firstTickIdInArray + stepsThatFollow - 1);)

        CLOG_C_VERBOSE(&self->log, "handleIncomingSteps: incoming step range %08X - %08X (count:%hhu)",
                       firstTickIdInArray, lastStepId, stepsThatFollow)

        // Drop old predicted steps if needed.
        size_t dropped = nbsStepsDropped(&participant->steps, firstTickIdInArray);
        if (dropped > 0) {
            if (dropped > 60U) {
                CLOG_C_WARN(&self->log, "client had a big gap in predicted steps %zu", dropped)
                // return -3;
            }
            CLOG_C_WARN(&self->log, "client step: dropped %zu steps. expected %08X, but got range from %08X to %08X",
                        dropped, participant->steps.expectedWriteId, firstTickIdInArray, lastStepId)
            // nimbleServerInsertForcedSteps(self, dropped);
        }

        size_t totalAddedStepsCount = 0;
        size_t totalOldStepsCount = 0;

        for (size_t i = 0; i < stepsThatFollow; ++i) {
            StepId stepId = firstTickIdInArray + (StepId) i;

            if (stepId < participant->steps.expectedWriteId) {
                totalOldStepsCount++;
            }

            int addedStepsCount = nimbleServerParticipantDeserializeSingleStep(participant, stepId, inStream);
            if (addedStepsCount < 0) {
                CLOG_C_SOFT_ERROR(&self->log, "client step: couldn't in-serialize single step")
                return addedStepsCount;
            }

            totalAddedStepsCount += (size_t) addedStepsCount;
        }

        if (totalAddedStepsCount == 0) {
            if (totalOldStepsCount > 0) {
                CLOG_C_NOTICE(&self->log, "only received %zu old predicted steps from %08X-%08X", totalOldStepsCount,
                              firstTickIdInArray, firstTickIdInArray + stepsThatFollow - 1)
            }

            if (self->warningAboutZeroAddedSteps++ % 4 == 0) {
                // CLOG_C_DEBUG(
                //   &self->log,
                // "Got a packet with old predicted steps. range: %08X - %08X, but is waiting for %08X. (Not
                // a " "problem unless it happens a lot)", firstStepId, lastStepId,
                // self->steps.expectedWriteId)
            }
        }

        if (totalAddedStepsCount > 0) {
            nimbleServerConnectionQualityAddedStepsToBuffer(&self->quality, (size_t) totalAddedStepsCount);

            StepId receivedUpToStepId = participant->steps.expectedWriteId - 1;
            if (receivedUpToStepId > self->highestReceivedStepId) {
                self->highestReceivedStepId = receivedUpToStepId;
                self->stepsInBufferCount = participant->steps.stepsCount;
            }
        }
    }

    return 0;
}
