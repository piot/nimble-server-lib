/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/nimble-server-lib
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/
#include "authoritative_steps.h"
#include <flood/in_stream.h>
#include <nimble-serialize/server_out.h>
#include <nimble-server/forced_step.h>
#include <nimble-server/participant_connection.h>
#include <nimble-server/participant_connections.h>
#include <nimble-server/transport_connection.h>
#include <nimble-steps-serialize/types.h>

#define NIMBLE_SERVER_LOGGING 1

static int composeOneAuthoritativeStep(NimbleServerParticipantConnections* connections, StepId lookingFor,
                                       uint8_t* composeStepBuffer, size_t maxLength, size_t* outSize)
{
    FldOutStream composeStream;
    fldOutStreamInit(&composeStream, composeStepBuffer, maxLength);
    fldOutStreamWriteUInt8(&composeStream, 0); // Participant count, that is filled in later in this function.
    StepId encounteredStepId;
    size_t foundParticipantCount = 0;
    uint8_t stepReadBuffer[1024];

    for (size_t i = 0; i < connections->capacityCount; ++i) {
        NimbleServerParticipantConnection* connection = &connections->connections[i];
        if (!connection->isUsed) {
            continue;
        }

        NbsSteps* steps = &connection->steps;

        size_t stepOctetCount;
        if (steps->stepsCount >= 1U) {
            int readStepOctetCount = nbsStepsRead(steps, &encounteredStepId, stepReadBuffer, 1024);

            // CLOG_VERBOSE("authenticate: connection %d, found step:%08X, octet count: %d", connection->id,
            // encounteredStepId, stepOctetCount);

            StepId connectionCanProvideId = encounteredStepId;

            if (readStepOctetCount < 0) {
                CLOG_C_SOFT_ERROR(&connection->log, "couldn't read from connection %d, server was hoping for step %08X",
                                  readStepOctetCount, lookingFor)
                return readStepOctetCount;
            }

            stepOctetCount = (size_t) readStepOctetCount;

            if (connectionCanProvideId != lookingFor) {
                CLOG_C_VERBOSE(&connection->log, "strange, wasn't the ID I was looking for. Wanted %08X, but got %08X",
                               lookingFor, connectionCanProvideId)
                int discardErr = nbsStepsDiscardUpTo(&connection->steps, lookingFor + 1);
                if (discardErr < 0) {
                    CLOG_C_ERROR(&connection->log, "could not discard including %d", discardErr)
                }

                ssize_t createdForcedStepOctetCount = nimbleServerCreateForcedStep(connection, stepReadBuffer, 1024);
                if (createdForcedStepOctetCount < 0) {
                    return (int) createdForcedStepOctetCount;
                }
                stepOctetCount = (size_t) createdForcedStepOctetCount;

                nimbleServerConnectionQualityAddedForcedSteps(&connection->quality, 1);
            } else {
                nimbleServerConnectionQualityProvidedUsableStep(&connection->quality);
            }
        } else {
            ssize_t createdForcedStepOctetCount = nimbleServerCreateForcedStep(connection, stepReadBuffer, 1024);
            if (createdForcedStepOctetCount < 0) {
                CLOG_NOTICE("%zu: could not create forced step", i)
                return (int) createdForcedStepOctetCount;
            }
            stepOctetCount = (size_t) createdForcedStepOctetCount;
            nimbleServerConnectionQualityAddedForcedSteps(&connection->quality, 1);
            CLOG_C_VERBOSE(&connection->log,
                           "no steps stored in connection %zu (%u). server is looking for %08X. inserting forced step",
                           i, connection->id, lookingFor)
        }

        int verifyResult = nbsStepsVerifyStep(stepReadBuffer, stepOctetCount);
        if (verifyResult < 0) {
            CLOG_C_SOFT_ERROR(&connection->log, "invalid step from connection %u lookingFor:%08X", connection->id,
                              lookingFor)
            return -6;
        }
        // CLOG_VERBOSE("connection %d: steps in buffer from %08X, count: %d (%08X)", i, steps->expectedReadId,
        // steps->stepsCount, steps->expectedWriteId);

        FldInStream stepInStream;
        fldInStreamInit(&stepInStream, stepReadBuffer, stepOctetCount);
        uint8_t localParticipantCount;
        fldInStreamReadUInt8(&stepInStream, &localParticipantCount);
        if (localParticipantCount == 0) {
            CLOG_C_ERROR(&connection->log, "not allowed to have zero participant count in message")
        }

        if (localParticipantCount != connection->participantReferences.participantReferenceCount) {
            CLOG_C_NOTICE(
                &connection->log, "connection %u should provide the appropriate number of participants %d vs %zu",
                connection->id, localParticipantCount, connection->participantReferences.participantReferenceCount)
        }

        uint8_t splitStepBuffer[64];
        for (size_t participantIndex = 0; participantIndex < localParticipantCount; ++participantIndex) {
            uint8_t participantId;
            fldInStreamReadUInt8(&stepInStream, &participantId);
            uint8_t localStepOctetCount = 0;
            uint8_t readLocalConnectState = NimbleSerializeStepTypeNormal;
            bool hasMask = participantId & 0x80;
            uint8_t mask = 0;
            if (hasMask) {
                fldInStreamReadUInt8(&stepInStream, &readLocalConnectState);
                participantId = participantId & 0x7f;
                mask = 0x80;
            } else {
                fldInStreamReadUInt8(&stepInStream, &localStepOctetCount);
                fldInStreamReadOctets(&stepInStream, splitStepBuffer, localStepOctetCount);
            }
#if NIMBLE_SERVER_LOGGING && 0
            CLOG_C_INFO(&connection->log, "step %08X connection %d. Read participant ID %d (octetCount %hhu)",
                        lookingFor, connection->id, participantId, localStepOctetCount)
#endif

            int hasParticipant = nimbleServerParticipantConnectionHasParticipantId(connection, participantId);
            if (!hasParticipant) {
                CLOG_C_SOFT_ERROR(&connection->log,
                                  "participant connection %u had no right to insert steps for participant %hhu",
                                  connection->id, participantId)
                continue;
            }
            fldOutStreamWriteUInt8(&composeStream, mask | participantId);
            if (mask) {
                fldOutStreamWriteUInt8(&composeStream, readLocalConnectState);
            } else {
                fldOutStreamWriteUInt8(&composeStream, localStepOctetCount);
                fldOutStreamWriteOctets(&composeStream, splitStepBuffer, localStepOctetCount);
            }
            // CLOG_C_INFO(&connection->log, "connection %d. Wrote participant ID %d (octetCount %d)", connection->id,
            // participantId, localStepOctetCount)
            foundParticipantCount++;
        }
    }
    composeStepBuffer[0] = (uint8_t) foundParticipantCount;
    *outSize = composeStream.pos;

    // CLOG_INFO("combined step done. participant count %zu, total octet count: %zu", foundParticipantCount,
    // composeStream.pos);

    return (int) foundParticipantCount;
}

static int maxPredictedStepContributionForConnections(NimbleServerParticipantConnections* connections,
                                                      StepId lookingFor, int* outConnectionsThatCouldNotContribute)
{
    int maxConnectionCanAdvanceStepCount = 0;
    int connectionCountThatCanNotContribute = 0;

    for (size_t i = 0u; i < connections->capacityCount; ++i) {
        const NimbleServerParticipantConnection* connection = &connections->connections[i];
        if (!connection->isUsed) {
            continue;
        }
        int connectionCanAdvanceStepCount = 0;
        if (connection->steps.stepsCount > 0u && connection->steps.expectedWriteId > lookingFor) {
            connectionCanAdvanceStepCount = (int) (connection->steps.expectedWriteId - lookingFor + 1u);
        } else {
            connectionCountThatCanNotContribute++;
        }

        if (connectionCanAdvanceStepCount > maxConnectionCanAdvanceStepCount) {
            maxConnectionCanAdvanceStepCount = connectionCanAdvanceStepCount;
        }
    }
    *outConnectionsThatCouldNotContribute = connectionCountThatCanNotContribute;

    return maxConnectionCanAdvanceStepCount;
}

static bool shouldComposeNewAuthoritativeStep(NimbleServerParticipantConnections* connections, StepId lookingFor)
{
    int connectionCountThatCouldNotContribute = 0;
    int availableSteps = maxPredictedStepContributionForConnections(connections, lookingFor,
                                                                    &connectionCountThatCouldNotContribute);

    bool shouldCompose = (availableSteps > 3 && connectionCountThatCouldNotContribute == 0) || availableSteps > 5;
    // CLOG_INFO("available steps for composing: %d couldNotContribute:%d result:%d", availableSteps,
    //         connectionCountThatCouldNotContribute, shouldCompose)

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

static bool shouldAdvanceAuthoritative(NimbleServerParticipantConnections* connections, NbsSteps* authoritativeSteps)
{
    return shouldComposeNewAuthoritativeStep(connections, authoritativeSteps->expectedWriteId) &&
           canAdvanceDueToDistanceFromLastState(authoritativeSteps);
}

/// Compose as many authoritative steps as possible
/// @param game game
/// @param connections connections to compose steps for
/// @return number of combined steps composed
int nimbleServerComposeAuthoritativeSteps(NimbleServerGame* game, NimbleServerParticipantConnections* connections)
{
    size_t writtenAuthoritativeSteps = 0;
    NbsSteps* authoritativeSteps = &game->authoritativeSteps;

#if NIMBLE_SERVER_LOGGING && defined CLOG_LOG_ENABLED
    StepId firstLookingFor = authoritativeSteps->expectedWriteId;
#endif

    while (shouldAdvanceAuthoritative(connections, authoritativeSteps)) {
        StepId lookingFor = authoritativeSteps->expectedWriteId;

        uint8_t composeStepBuffer[1024];
        size_t authoritativeStepOctetCount;
        int foundParticipantCount = composeOneAuthoritativeStep(connections, lookingFor, composeStepBuffer, 1024,
                                                                &authoritativeStepOctetCount);
        if (foundParticipantCount < 0) {
            CLOG_C_SOFT_ERROR(&game->log, "authoritative: couldn't compose a authoritative step")
            return foundParticipantCount;
        }

        if ((size_t) foundParticipantCount != game->participants.participantCount) {
            // CLOG_ERROR("we couldn't compose an authoritative step, even though they said everyone is ready. we needed
            // %zu and got %zu", game->room->participants.participantCount, foundParticipantCount);
        }

        if (foundParticipantCount == 0) {
            CLOG_C_SOFT_ERROR(&game->log, "authoritative: can't have zero participant count")
            return -48;
        }

        int octetsWritten = nbsStepsWrite(authoritativeSteps, lookingFor, composeStepBuffer,
                                          authoritativeStepOctetCount);
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
