/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/nimble-server-lib
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/

#include "authoritative_steps.h"
#include <flood/in_stream.h>
#include <nimble-serialize/server_out.h>
#include <nimble-server/local_parties.h>
#include <nimble-server/local_party.h>
#include <nimble-server/participant.h>
#include <nimble-server/participants.h>
#include <nimble-server/transport_connection.h>
#include <nimble-steps-serialize/types.h>

#define NIMBLE_SERVER_LOGGING 1

/// Composes one authoritative steps from the collection of participants.
/// @param participants all the participants to combine steps from .
/// @param lookingFor the stepId to compose
/// @param composeStepBuffer the buffer to use for composing.
/// @param maxLength maximum size of the composeStepBuffer
/// @return the number of octets written or negative on error
static ssize_t composeOneAuthoritativeStep(NimbleServerParticipants* participants, StepId lookingFor,
                                           uint8_t* composeStepBuffer, size_t maxLength)
{
    FldOutStream composeStream;
    fldOutStreamInit(&composeStream, composeStepBuffer, maxLength);
    fldOutStreamWriteUInt8(&composeStream, (uint8_t) participants->participantCount);

    uint8_t stepReadBuffer[1024];

    CLOG_EXECUTE(size_t foundParticipantCount = 0;)
    for (size_t i = 0; i < participants->participantCapacity; ++i) {
        NimbleServerParticipant* participant = &participants->participants[i];
        if (!participant->isUsed) {
            continue;
        }
        CLOG_EXECUTE(foundParticipantCount++;)
        NbsSteps* steps = &participant->steps;

        uint8_t mask = 0x00;
        NimbleSerializeStepType stepType = NimbleSerializeStepTypeNormal;

        uint8_t readStepOctetCountToUse = 0;
        {
            int readStepOctetCount = nbsStepsReadExactStepId(steps, lookingFor, stepReadBuffer, 1024);
            if (readStepOctetCount < 0) {
                if (readStepOctetCount == NimbleStepErrCollectionIsEmpty) {
                    nimbleServerConnectionQualityAddedForcedSteps(&participant->inParty->quality, 1);
                    CLOG_C_VERBOSE(&participant->log,
                                   "no steps stored (party: %u). server is looking for %08X. using a forced step",
                                   participant->inParty->id, lookingFor)
                    stepType = NimbleSerializeStepTypeStepNotProvidedInTime;
                    readStepOctetCount = 0;
                } else {
                    CLOG_C_ERROR(&participant->log, "steps for participant is corrupt. error %d", readStepOctetCount)
                }
            } else {
                nimbleServerConnectionQualityProvidedUsableStep(&participant->inParty->quality);
            }
            readStepOctetCountToUse = tc_convert_uint8_t_from_ssize(readStepOctetCount);
        }

        // CLOG_VERBOSE("party %d: steps in buffer from %08X, count: %d (%08X)", i, steps->expectedReadId,
        // steps->stepsCount, steps->expectedWriteId);

        switch (participant->state) {
            case NimbleServerParticipantStateWaitingForRejoin:
                stepType = NimbleSerializeStepTypeWaitingForReJoin;
                break;
            case NimbleServerParticipantStateJustJoined:
                stepType = NimbleSerializeStepTypeJoined;
                participant->state = NimbleServerParticipantStateNormal;
                break;
            case NimbleServerParticipantStateLeaving:
                nimbleServerParticipantsDestroy(participants, participant->id);
                stepType = NimbleSerializeStepTypeLeft;
                break;
            case NimbleServerParticipantStateNormal:
                break;
            case NimbleServerParticipantStateDestroyed:
                break;
        }

        if (stepType != NimbleSerializeStepTypeNormal) {
            mask = 0x80;
        }
        int serializeError = fldOutStreamWriteUInt8(&composeStream, mask | participant->id);
        if (serializeError < 0) {
            return serializeError;
        }
        if (mask) {
            fldOutStreamWriteUInt8(&composeStream, (uint8_t) stepType);
            if (stepType == NimbleSerializeStepTypeJoined) {
                fldOutStreamWriteUInt8(&composeStream, participant->inParty->id);
            }
        }

        if (stepType == NimbleSerializeStepTypeNormal || stepType == NimbleSerializeStepTypeJoined) {
            fldOutStreamWriteUInt8(&composeStream, readStepOctetCountToUse);
            fldOutStreamWriteOctets(&composeStream, stepReadBuffer,
                                    tc_convert_uint8_t_from_ssize(readStepOctetCountToUse));
        }

        CLOG_C_VERBOSE(&participant->log, "wrote authoritative step %08X (octetCount %d) (%s)",
                       lookingFor, readStepOctetCountToUse, nimbleSerializeStepTypeToString(stepType))
    }
    CLOG_ASSERT(foundParticipantCount == participants->participantCount,
                "did not find the same amount of participants as in participantCount")

    CLOG_C_VERBOSE(&participants->log, "authoritative step %08X done. participant count %zu, total octet count: %zu",
                   lookingFor, foundParticipantCount, composeStream.pos)

    return (ssize_t) composeStream.pos;
}

/// Checks the maximum number of steps any participant has, and the number of participants that can not contribute at
/// all.
/// @param participants participants to consider
/// @param lookingFor the step ID that is desired to be composed.
/// @param countOfParticipantsThatCouldNotContribute the number of participants that could not contribute
/// @return maximum number of steps that can be composed if absolutely needed.
static size_t maxPredictedStepContributionForParticipants(NimbleServerParticipants* participants, StepId lookingFor,
                                                          size_t* countOfParticipantsThatCouldNotContribute)
{
    size_t maxConnectionCanAdvanceStepCount = 0;
    size_t participantCountThatCanNotContribute = 0;

    for (size_t i = 0u; i < participants->participantCapacity; ++i) {
        const NimbleServerParticipant* participant = &participants->participants[i];
        if (!participant->isUsed) {
            continue;
        }
        size_t participantCanAdvanceStepCount = 0;
        if (participant->steps.stepsCount > 0u && participant->steps.expectedWriteId > lookingFor) {
            participantCanAdvanceStepCount = (size_t) (participant->steps.expectedWriteId - lookingFor + 1u);
        } else {
            participantCountThatCanNotContribute++;
        }

        if (participantCanAdvanceStepCount > maxConnectionCanAdvanceStepCount) {
            maxConnectionCanAdvanceStepCount = participantCanAdvanceStepCount;
        }
    }
    *countOfParticipantsThatCouldNotContribute = participantCountThatCanNotContribute;

    return maxConnectionCanAdvanceStepCount;
}

static bool shouldComposeNewAuthoritativeStep(NimbleServerParticipants* participants, StepId lookingFor)
{
    size_t connectionCountThatCouldNotContribute = 0;
    size_t maxCountStepAheadForSomeParticipant = maxPredictedStepContributionForParticipants(
        participants, lookingFor, &connectionCountThatCouldNotContribute);

    bool shouldCompose = (maxCountStepAheadForSomeParticipant > 3 && connectionCountThatCouldNotContribute == 0) ||
                         maxCountStepAheadForSomeParticipant > 5;
    CLOG_C_VERBOSE(&participants->log,
                   "available steps for composing:%zu (%08X-%08X) couldNotContribute:%zu willCompose:%d",
                   maxCountStepAheadForSomeParticipant, lookingFor,
                   (StepId) (lookingFor + maxCountStepAheadForSomeParticipant - 1),
                   connectionCountThatCouldNotContribute, shouldCompose)

    return shouldCompose;
}

static bool canAdvanceDueToDistanceFromLastState(NbsSteps* authoritativeSteps)
{
    const size_t maxAuthoritativeStepCountSinceState = NBS_WINDOW_SIZE / 2;
    bool allowed = authoritativeSteps->stepsCount < maxAuthoritativeStepCountSinceState;
    if (!allowed) {
        CLOG_WARN("we have too many steps in authoritative buffer (%zu). Waiting for state from client or "
                  "locally on server",
                  authoritativeSteps->stepsCount)
    }
    return allowed;
}

static bool shouldAdvanceAuthoritative(NimbleServerParticipants* participants, NbsSteps* authoritativeSteps)
{
    return shouldComposeNewAuthoritativeStep(participants, authoritativeSteps->expectedWriteId) &&
           canAdvanceDueToDistanceFromLastState(authoritativeSteps);
}

/// Compose as many authoritative steps as possible
/// @param game game to compose an authoritative steps for
/// @return number of combined steps composed
int nimbleServerComposeAuthoritativeSteps(NimbleServerGame* game)
{
    size_t writtenAuthoritativeSteps = 0;
    NbsSteps* authoritativeSteps = &game->authoritativeSteps;

#if NIMBLE_SERVER_LOGGING && defined CLOG_LOG_ENABLED
    StepId firstLookingFor = authoritativeSteps->expectedWriteId;
#endif

    while (shouldAdvanceAuthoritative(&game->participants, authoritativeSteps)) {
        StepId lookingFor = authoritativeSteps->expectedWriteId;

        uint8_t composeStepBuffer[1024];
        ssize_t authoritativeStepOctetCount = composeOneAuthoritativeStep(&game->participants, lookingFor,
                                                                          composeStepBuffer, 1024);
        if (authoritativeStepOctetCount <= 0) {
            CLOG_C_SOFT_ERROR(&game->log, "authoritative: couldn't compose a authoritative step")
            return 0;
        }

        int octetsWritten = nbsStepsWrite(authoritativeSteps, lookingFor, composeStepBuffer,
                                          (size_t) authoritativeStepOctetCount);
        if (octetsWritten < 0) {
            CLOG_C_SOFT_ERROR(&game->log, "authoritative: couldn't write")
            return octetsWritten;
        }

        writtenAuthoritativeSteps++;
    }

#if NIMBLE_SERVER_LOGGING && defined CLOG_LOG_ENABLED
    if (writtenAuthoritativeSteps > 0) {
        CLOG_C_VERBOSE(&game->log, "authoritative: written steps from %08X to %zX (%zX)", firstLookingFor,
                       firstLookingFor + writtenAuthoritativeSteps - 1, writtenAuthoritativeSteps)
    }
#endif
    return (int) writtenAuthoritativeSteps;
}
