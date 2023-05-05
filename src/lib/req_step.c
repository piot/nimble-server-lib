/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <nimble-server/req_step.h>

#include "nimble-server/server.h"
#include <flood/in_stream.h>
#include <nimble-serialize/server_out.h>
#include <nimble-server/forced_step.h>
#include <nimble-server/participant_connection.h>
#include <nimble-steps-serialize/in_serialize.h>
#include <nimble-steps-serialize/pending_in_serialize.h>
#include <nimble-steps-serialize/pending_out_serialize.h>

#define NBD_LOGGING 1

static int composeAuthoritativeStep(NbdParticipantConnections* connections, StepId lookingFor,
                                    uint8_t* composeStepBuffer, size_t maxLength, size_t* outSize)
{
    FldOutStream composeStream;
    fldOutStreamInit(&composeStream, composeStepBuffer, maxLength);
    fldOutStreamWriteUInt8(&composeStream,0); // Participant count, that is filled in later in this function.
    StepId encounteredStepId;
    size_t foundParticipantCount = 0;
    uint8_t stepReadBuffer[1024];

    for (size_t i = 0; i < connections->capacityCount; ++i) {
        NbdParticipantConnection* connection = &connections->connections[i];
        if (!connection->isUsed) {
            continue;
        }
        NbsSteps* steps = &connection->steps;

        int stepOctetCount;
        if (steps->stepsCount >= 1u) {
            stepOctetCount = nbsStepsRead(steps, &encounteredStepId, stepReadBuffer, 1024);

            // CLOG_VERBOSE("authenticate: connection %d, found step:%08X, octet count: %d", connection->id,
            // encounteredStepId, stepOctetCount);

            StepId connectionCanProvideId = encounteredStepId;

            if (stepOctetCount < 0) {
                CLOG_C_SOFT_ERROR(&connection->log, "couldn't read from connection %d, server was hoping for step %08X",
                                  stepOctetCount, lookingFor)
                return stepOctetCount;
            }

            if (connectionCanProvideId != lookingFor) {
                CLOG_C_VERBOSE(&connection->log,
                                  "strange, wasn't the ID I was looking for. Wanted %08X, but got %08X", lookingFor,
                                  connectionCanProvideId)
                int discardErr = nbsStepsDiscardUpTo(&connection->steps, lookingFor + 1);
                if (discardErr < 0) {
                    CLOG_C_ERROR(&connection->log, "could not discard including %d", discardErr)
                }

                stepOctetCount = nbdCreateForcedStep(connection, stepReadBuffer, 1024);
                connection->forcedStepInRowCounter++;
            } else {
                connection->forcedStepInRowCounter = 0;
            }
        } else {
            stepOctetCount = nbdCreateForcedStep(connection, stepReadBuffer, 1024);
            connection->forcedStepInRowCounter++;
            CLOG_C_VERBOSE(&connection->log,
                        "no steps stored in connection %zu (%u). server is looking for %08X. inserting forced step", i,
                        connection->id, lookingFor)
        }

        int verifyResult = nbsStepsVerifyStep(stepReadBuffer, stepOctetCount);
        if (verifyResult < 0) {
            CLOG_C_SOFT_ERROR(&connection->log, "invalid step from connection %d lookingFor:%08X", connection->id,
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
            CLOG_C_ERROR(&connection->log, "not allowed to have zero participant count in message");
        }

        if (localParticipantCount != connection->participantReferences.participantReferenceCount) {
            CLOG_C_NOTICE(
                &connection->log, "connection %d should provide the appropriate number of participants %d vs %zu",
                connection->id, localParticipantCount, connection->participantReferences.participantReferenceCount)
        }

        uint8_t splitStepBuffer[64];
        for (size_t participantIndex = 0; participantIndex < localParticipantCount; ++participantIndex) {
            uint8_t participantId;
            fldInStreamReadUInt8(&stepInStream, &participantId);
            if (participantId == 0) {
                CLOG_C_SOFT_ERROR(&connection->log, "participantId zero is reserved")
            }
            uint8_t localStepOctetCount;
            fldInStreamReadUInt8(&stepInStream, &localStepOctetCount);
            fldInStreamReadOctets(&stepInStream, splitStepBuffer, localStepOctetCount);
#if NBD_LOGGING && 0
            CLOG_C_INFO(&connection->log, "step %08X connection %d. Read participant ID %d (octetCount %hhu)",
                        lookingFor, connection->id, participantId, localStepOctetCount)
#endif

            int hasParticipant = nbdParticipantConnectionHasParticipantId(connection, participantId);
            if (!hasParticipant) {
                CLOG_C_SOFT_ERROR(&connection->log,
                                  "participant connection %d had no right to insert steps for participant %hhu",
                                  connection->id, participantId)
                continue;
            }

            fldOutStreamWriteUInt8(&composeStream, participantId);
            fldOutStreamWriteUInt8(&composeStream, localStepOctetCount);
            fldOutStreamWriteOctets(&composeStream, splitStepBuffer, localStepOctetCount);
            //CLOG_C_INFO(&connection->log, "connection %d. Wrote participant ID %d (octetCount %d)", connection->id, participantId, localStepOctetCount)
            foundParticipantCount++;
        }
    }
    composeStepBuffer[0] = foundParticipantCount;
    *outSize = composeStream.pos;

    //CLOG_INFO("combined step done. participant count %zu, total octet count: %zu", foundParticipantCount, composeStream.pos);

    return (int) foundParticipantCount;
}

static int maxPredictedStepContributionForConnections(NbdParticipantConnections* connections, StepId lookingFor,
                                                      int* outConnectionsThatCouldNotContribute)
{
    int maxConnectionCanAdvanceStepCount = 0;
    int connectionCountThatCanNotContribute = 0;

    for (size_t i = 0u; i < connections->capacityCount; ++i) {
        const NbdParticipantConnection* connection = &connections->connections[i];
        if (!connection->isUsed) {
            continue;
        }
        int connectionCanAdvanceStepCount = 0;
        if (connection->steps.stepsCount > 0u && connection->steps.expectedWriteId > lookingFor) {
            connectionCanAdvanceStepCount = connection->steps.expectedWriteId - lookingFor + 1u;
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

static bool shouldComposeNewAuthoritativeStep(NbdParticipantConnections* connections, StepId lookingFor)
{
    int connectionCountThatCouldNotContribute = 0;
    int availableSteps = maxPredictedStepContributionForConnections(connections, lookingFor,
                                                                    &connectionCountThatCouldNotContribute);

    bool shouldCompose = (availableSteps > 0 && connectionCountThatCouldNotContribute == 0) || availableSteps > 3;
    //CLOG_INFO("available steps for composing: %d couldNotContribute:%d result:%d", availableSteps,
      //        connectionCountThatCouldNotContribute, shouldCompose)

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

static bool shouldAdvanceAuthoritative(NbdParticipantConnections* connections, NbsSteps* authoritativeSteps)
{
    return shouldComposeNewAuthoritativeStep(connections, authoritativeSteps->expectedWriteId) &&
           canAdvanceDueToDistanceFromLastState(authoritativeSteps);
}

static void nbdGameShowReport(NbdGame* game, NbdParticipantConnections* connections)
{
    NbsSteps* steps = &game->authoritativeSteps;
    CLOG_C_INFO(&game->log, "Authoritative: steps %08X to %08X (count: %d) latestState: %08X", steps->expectedReadId,
                steps->expectedWriteId - 1, steps->stepsCount, game->latestState.stepId)

    for (size_t i = 0u; i < connections->connectionCount; ++i) {
        const NbdParticipantConnection* connection = &connections->connections[i];
        if (connection->steps.stepsCount == 0) {
            CLOG_C_INFO(&game->log, " .. con#%d: no steps, lastStepId: %08X", connection->id,
                        connection->steps.expectedWriteId - 1)

        } else {
            CLOG_C_INFO(&game->log, " .. con#%d: steps: %08X to %08X count:%zu", connection->id,
                        connection->steps.expectedReadId, connection->steps.expectedWriteId - 1,
                        connection->steps.stepsCount)
        }
    }
    CLOG_C_INFO(&game->log, "Authoritative: -----")
}

static int advanceAuthoritativeAsFarAsWeCan(NbdGame* game, NbdParticipantConnections* connections)
{
    size_t writtenAuthoritativeSteps = 0;
    NbsSteps* authoritativeSteps = &game->authoritativeSteps;

#if NBD_LOGGING && CLOG_LOG_ENABLED
    StepId firstLookingFor = authoritativeSteps->expectedWriteId;
#endif
    // nbdGameShowReport(game, connections);

    while (shouldAdvanceAuthoritative(connections, authoritativeSteps)) {
        StepId lookingFor = authoritativeSteps->expectedWriteId;

        uint8_t composeStepBuffer[1024];
        size_t authoritativeStepOctetCount;
        int foundParticipantCount = composeAuthoritativeStep(connections, lookingFor, composeStepBuffer, 1024,
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
        CLOG_C_VERBOSE(&game->log, "authenticate: we have written %08X", lookingFor);

        //nbdGameShowReport(game, connections);

        writtenAuthoritativeSteps++;
    }

#if NBD_LOGGING && CLOG_LOG_ENABLED
    if (writtenAuthoritativeSteps > 0) {
        CLOG_C_VERBOSE(&game->log, "authoritative: written steps from %08X to %zX (%zX)", firstLookingFor,
                       firstLookingFor + writtenAuthoritativeSteps - 1, writtenAuthoritativeSteps)
    }
#endif
    return (int) writtenAuthoritativeSteps;
}

static int discardAuthoritativeStepsIfBufferGettingFull(NbdGame* foundGame)
{
    size_t authoritativeStepCount = foundGame->authoritativeSteps.stepsCount;
    size_t maxCapacity = NBS_WINDOW_SIZE / 3;

    if (authoritativeStepCount > maxCapacity) {
        size_t authoritativeToDrop = authoritativeStepCount - maxCapacity;
        int err = nbsStepsDiscardCount(&foundGame->authoritativeSteps, authoritativeToDrop);
        if (err < 0) {
            return err;
        }
    }

    return 0;
}

static int handleIncomingSteps(NbdGame* foundGame, NbdParticipantConnections* connections, FldInStream* inStream,
                               NbdTransportConnection* transportConnection, StepId* outClientWaitingForStepId,
                               uint64_t* outReceiveMask, uint16_t* receivedTimeFromClient)
{

    StepId clientWaitingForStepId;
    uint64_t receiveMask;

    int errorCode = nbsPendingStepsInSerializeHeader(inStream, &clientWaitingForStepId, &receiveMask, receivedTimeFromClient);
    if (errorCode < 0) {
        CLOG_C_SOFT_ERROR(&transportConnection->log, "client step: couldn't in-serialize pending steps")
        return errorCode;
    }

    *outClientWaitingForStepId = clientWaitingForStepId;
    *outReceiveMask = receiveMask;

    CLOG_C_VERBOSE(&transportConnection->log, "handleIncomingSteps: client %d is awaiting step %08X receiveMask: %08X",
                   transportConnection->transportConnectionId, clientWaitingForStepId, receiveMask)

    size_t stepsThatFollow;
    StepId firstStepId;
    errorCode = nbsStepsInSerializeHeader(inStream, &firstStepId, &stepsThatFollow);
    if (errorCode < 0) {
        CLOG_C_SOFT_ERROR(&transportConnection->log, "client step: couldn't in-serialize steps")
        return errorCode;
    }

    NbdParticipantConnection* foundParticipantConnection = transportConnection->assignedParticipantConnection;
    if (foundParticipantConnection == 0) {
        return 0;
    }

    CLOG_C_VERBOSE(&foundParticipantConnection->log,
                   "handleIncomingSteps: client %d incoming step %08X, %zu steps follow",
                   foundParticipantConnection->id, firstStepId, stepsThatFollow)

    size_t dropped = nbsStepsDropped(&foundParticipantConnection->steps, firstStepId);
    if (dropped > 0) {
        if (dropped > 60) {
            CLOG_C_WARN(&foundParticipantConnection->log, "dropped %d", dropped)
            return -3;
        }
        CLOG_C_WARN(&foundParticipantConnection->log,
                    "client step: dropped %zu steps. expected %08X, but got from %08X to %08X", dropped,
                    foundParticipantConnection->steps.expectedWriteId, firstStepId, firstStepId + stepsThatFollow - 1)
        nbdInsertForcedSteps(foundParticipantConnection, dropped);
    }

    int addedStepsCount = nbsStepsInSerialize(inStream, &foundParticipantConnection->steps, firstStepId,
                                              stepsThatFollow);
    if (addedStepsCount < 0) {
        return addedStepsCount;
    }

    int discardErr = discardAuthoritativeStepsIfBufferGettingFull(foundGame);
    if (discardErr < 0) {
        return discardErr;
    }

    int advanceCount = advanceAuthoritativeAsFarAsWeCan(foundGame, connections);
    if (advanceCount < 0) {
        return advanceCount;
    }

    return advanceCount;
}

static void showStats(NbdTransportConnection* transportConnection)
{
#define DEBUG_COUNT (128)
    char debug[DEBUG_COUNT];

    NbdParticipantConnection* foundParticipantConnection = transportConnection->assignedParticipantConnection;

    if (foundParticipantConnection) {
        tc_snprintf(debug, DEBUG_COUNT, "server: conn %d step count in incoming buffer",
                    foundParticipantConnection->id);
        statsIntDebug(&foundParticipantConnection->incomingStepCountInBufferStats, debug, "steps");
    }

    tc_snprintf(debug, DEBUG_COUNT, "server: conn %d steps behind authoritative (latency)",
                transportConnection->transportConnectionId);
    statsIntDebug(&transportConnection->stepsBehindStats, debug, "steps");
}

static void updateStats(NbdTransportConnection* transportConnection, NbdGame* foundGame, StepId clientWaitingForStepId)
{
    NbdParticipantConnection* foundParticipantConnection = transportConnection->assignedParticipantConnection;
    if (foundParticipantConnection != 0) {
        statsIntAdd(&foundParticipantConnection->incomingStepCountInBufferStats,
                    foundParticipantConnection->steps.stepsCount);
    }

    size_t stepsBehindForClient = foundGame->authoritativeSteps.expectedWriteId - clientWaitingForStepId;
    statsIntAdd(&transportConnection->stepsBehindStats, stepsBehindForClient);
}

static int sendStepRanges(FldOutStream* outStream, NbdTransportConnection* transportConnection, NbdGame* foundGame,
                          StepId clientWaitingForStepId, uint64_t receiveMask, uint16_t receivedTimeFromClient)
{
#define MAX_RANGES_COUNT (3)
    const int maxStepsCount = 8;
    NbsPendingRange ranges[MAX_RANGES_COUNT + 1];

    StepId lastReceivedStepFromClient = 0;
    size_t bufferDelta = 0;
    int authoritativeBufferDelta = 0;

    bool moreDebug = true; // (transportConnection->id == 0);
    int rangeCount = 0;

    if (clientWaitingForStepId < foundGame->authoritativeSteps.expectedReadId) {
        CLOG_C_NOTICE(&transportConnection->log,
                      "client wants to get authoritative %08X, but we only can provide the earliest %08X",
                      clientWaitingForStepId, foundGame->authoritativeSteps.expectedReadId)
        rangeCount = 0;
    } else {
        rangeCount = nbsPendingStepsRanges(clientWaitingForStepId, foundGame->authoritativeSteps.expectedWriteId,
                                           receiveMask, ranges, MAX_RANGES_COUNT, maxStepsCount);
        if (rangeCount < 0) {
            CLOG_C_VERBOSE(&transportConnection->log, "ranges error")
            return rangeCount;
        }

        if (foundGame->authoritativeSteps.expectedWriteId > transportConnection->nextAuthoritativeStepIdToSend) {
            // CLOG_INFO("client waiting for %0lX, game authoritative stepId is at %0lX", clientWaitingForStepId,
            //        foundGame->authoritativeSteps.expectedWriteId);

            size_t availableCount = foundGame->authoritativeSteps.expectedWriteId -
                                    transportConnection->nextAuthoritativeStepIdToSend;
            const size_t redundancyStepCount = 5;
            if (availableCount > redundancyStepCount) {
                availableCount = redundancyStepCount;
            }
            ranges[rangeCount].startId = transportConnection->nextAuthoritativeStepIdToSend;
            ranges[rangeCount].count = availableCount;

            StepId maxStepIdToSend = clientWaitingForStepId + NBS_WINDOW_SIZE - 1;
            if (maxStepIdToSend >= foundGame->authoritativeSteps.expectedWriteId) {
                maxStepIdToSend = foundGame->authoritativeSteps.expectedWriteId;
            }

            if (transportConnection->nextAuthoritativeStepIdToSend + redundancyStepCount < maxStepIdToSend) {
                transportConnection->nextAuthoritativeStepIdToSend += redundancyStepCount;
            }
            CLOG_C_VERBOSE(&transportConnection->log, "add continuation range %08X %zu", ranges[rangeCount].startId,
                           ranges[rangeCount].count);
            rangeCount++;
        } else {
            CLOG_C_VERBOSE(&transportConnection->log,
                           "no need to force any range. we have %08X and client wants %08X iteratorIsAt: %08X",
                           foundGame->authoritativeSteps.expectedWriteId - 1, clientWaitingForStepId,
                           transportConnection->nextAuthoritativeStepIdToSend);
        }
        /*
            nbsPendingStepsRangesDebugOutput(ranges, "server serialize out initial", rangeCount,
           transportConnection->log);
        */

        if (rangeCount == 0) {
            transportConnection->noRangesToSendCounter++;
            if (transportConnection->noRangesToSendCounter > 2) {
                int noticeTime = 0; // transportConnection->noRangesToSendCounter % 2;
                if (noticeTime == 0) {
                    CLOG_C_NOTICE(&transportConnection->log, "no ranges to send for %d ticks, suspicious",
                                  transportConnection->noRangesToSendCounter);
                }
            }
        } else {
            transportConnection->noRangesToSendCounter = 0;
        }

        NbdParticipantConnection* foundParticipantConnection = transportConnection->assignedParticipantConnection;
        if (foundParticipantConnection != 0) {
            lastReceivedStepFromClient = foundParticipantConnection->steps.expectedWriteId - 1;
            bufferDelta = foundParticipantConnection->steps.stepsCount;
            authoritativeBufferDelta = (int) foundParticipantConnection->steps.expectedWriteId -
                                       (int) foundGame->authoritativeSteps.expectedWriteId;
        }
    }

    nimbleSerializeServerOutStepHeader(outStream, lastReceivedStepFromClient, bufferDelta, authoritativeBufferDelta, receivedTimeFromClient);

    if (moreDebug) {
        nbsPendingStepsRangesDebugOutput(ranges, "server serialize out", rangeCount, transportConnection->log);
    }

    return nbsPendingStepsSerializeOutRanges(outStream, &foundGame->authoritativeSteps, ranges, rangeCount);
}

int nbdReqGameStep(NbdGame* foundGame, NbdTransportConnection* transportConnection,
                   NbdParticipantConnections* connections, FldInStream* inStream, FldOutStream* outStream)
{
    StepId clientWaitingForStepId;
    uint64_t receiveMask;
    uint16_t receivedTimeFromClient;

    int errorCode = handleIncomingSteps(foundGame, connections, inStream, transportConnection, &clientWaitingForStepId,
                                        &receiveMask, &receivedTimeFromClient);
    if (errorCode < 0) {
        CLOG_C_SOFT_ERROR(&transportConnection->log, "problem handling incoming step:%d", errorCode);
        return errorCode;
    }

    updateStats(transportConnection, foundGame, clientWaitingForStepId);
    if ((transportConnection->debugCounter++ % 100) == 0) {
        showStats(transportConnection);
    }

    return sendStepRanges(outStream, transportConnection, foundGame, clientWaitingForStepId, receiveMask, receivedTimeFromClient);
}
