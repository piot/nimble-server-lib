/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <nimble-server/req_step.h>

#include <clog/clog.h>
#include <flood/in_stream.h>
#include <flood/out_stream.h>
#include <nimble-serialize/server_out.h>
#include <nimble-server/default_step.h>
#include <nimble-server/game.h>
#include <nimble-server/participant_connection.h>
#include <nimble-steps-serialize/in_serialize.h>
#include <nimble-steps-serialize/pending_in_serialize.h>
#include <nimble-steps-serialize/pending_out_serialize.h>

#define NBD_LOGGING 1

static int checkEveryoneHasAStepAndDisconnectIfNeeded(NbdParticipantConnections* connections, StepId stepIdThatWeNeed)
{
    for (size_t i = 0; i < connections->capacityCount; ++i) {
        NbdParticipantConnection* connection = &connections->connections[i];
        if (!connection->isUsed) {
            continue;
        }
        NbsSteps* steps = &connection->steps;

        if (steps->stepsCount == 0) {
            CLOG_C_VERBOSE(&connections->log, "waiting for connection %d: had no steps", connection->id);
            steps->waitCounter++;
            if (steps->waitCounter > 30) {
                // connection->owner = 0;
            }
            return 0;
        }

        if (stepIdThatWeNeed > steps->expectedReadId) {
            CLOG_C_WARN(&connections->log, "we had to skip ahead. Step %08X is next, but server needs %08X",
                        steps->expectedReadId, stepIdThatWeNeed)
            nbsStepsDiscardUpTo(steps, stepIdThatWeNeed);
        }

        if (steps->stepsCount == 0) {
            CLOG_C_VERBOSE(&connections->log,
                           "waiting for connection %d: had no steps after discard", connection->id);
            steps->waitCounter++;
            if (steps->waitCounter > 30) {
                // connection->owner = 0;
            }

            return 0;
        }

        if (steps->expectedReadId != stepIdThatWeNeed) {
            CLOG_C_WARN(&connections->log, "we want %08X from the client, but it has stored %08X", stepIdThatWeNeed,
                      steps->expectedReadId)
            return 0;
        }
    }

    return 1;
}

static int composeAuthoritativeStep(NbdParticipantConnections* connections, StepId lookingFor,
                                    uint8_t* composeStepBuffer, size_t maxLength, size_t* outSize)
{
    FldOutStream composeStream;
    fldOutStreamInit(&composeStream, composeStepBuffer, maxLength);
    fldOutStreamWriteUInt8(&composeStream,
                           0); // Participant count, that is filled in later in this function.
    StepId encounteredStepId;
    size_t foundParticipantCount = 0;
    uint8_t stepReadBuffer[1024];
    // All connections had something to contribute, lets advance one
    for (size_t i = 0; i < connections->capacityCount; ++i) {
        NbdParticipantConnection* connection = &connections->connections[i];
        if (!connection->isUsed) {
            continue;
        }
        NbsSteps* steps = &connection->steps;

        int stepOctetCount;
        if (steps->stepsCount < 1) {
            //  nbdInsertDefaultSteps(connection, 1);
            CLOG_C_WARN(&connection->log, "no steps stored in connection %zu (%u). server is looking for %08X", i, connection->id,
                      lookingFor)
        }
        stepOctetCount = nbsStepsRead(steps, &encounteredStepId, stepReadBuffer, 1024);

        // CLOG_VERBOSE("authenticate: connection %d, found step:%08X, octet count: %d", connection->id,
        // encounteredStepId, stepOctetCount);

        StepId connectionCanProvideId = encounteredStepId;

        if (stepOctetCount < 0) {
            CLOG_C_SOFT_ERROR(&connection->log, "couldn't read from connection %d, server was hoping for step %08X", stepOctetCount,
                            lookingFor)
            return stepOctetCount;
        }
        if (stepOctetCount < 1) {
            CLOG_C_SOFT_ERROR(&connection->log, "it should have something to contribute")
            return -4;
        }

        if (connectionCanProvideId != lookingFor) {
            CLOG_C_SOFT_ERROR(&connection->log, "strange, wasn't the ID I was looking for. Wanted %08X, but got %08X", lookingFor,
                            connectionCanProvideId)
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
            CLOG_C_NOTICE(&connection->log, "connection %d should provide the appropriate number of participants %d vs %zu", connection->id,
                        localParticipantCount, connection->participantReferences.participantReferenceCount)
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
#if NBD_LOGGING
            CLOG_C_VERBOSE(&connection->log, "connection %d. Read participant ID %d (octetCount %hhu)", connection->id, participantId,
                         localStepOctetCount)
#endif

            int hasParticipant = nbdParticipantConnectionHasParticipantId(connection, participantId);
            if (!hasParticipant) {
                CLOG_C_SOFT_ERROR(&connection->log, "participant connection %d had no right to insert steps for participant %hhu",
                                connection->id, participantId)
                continue;
            }

            fldOutStreamWriteUInt8(&composeStream, participantId);
            fldOutStreamWriteUInt8(&composeStream, localStepOctetCount);
            fldOutStreamWriteOctets(&composeStream, splitStepBuffer, localStepOctetCount);
            // CLOG_VERBOSE("connection %d. Wrote participant ID %d (octetCount %hhu)", connection->id, participantId,
            // localStepOctetCount)
            foundParticipantCount++;
        }
    }
    composeStepBuffer[0] = foundParticipantCount;
    *outSize = composeStream.pos;

    // CLOG_VERBOSE("combined. participant count %zu, total octet count: %zu", foundParticipantCount,
    // composeStream.pos);

    return (int) foundParticipantCount;
}

static int advanceAuthoritativeAsFarAsWeCan(NbdGame* game, NbdParticipantConnections* connections)
{
    size_t writtenAuthoritativeSteps = 0;
    NbsSteps* authoritativeSteps = &game->authoritativeSteps;

    const size_t maxAuthoritativeStepCountSinceState = 240;
#if NBD_LOGGING && CLOG_LOG_ENABLED
    StepId firstLookingFor = authoritativeSteps->expectedWriteId;
#endif

    while (1) {
        if (authoritativeSteps->stepsCount >= maxAuthoritativeStepCountSinceState) {
            CLOG_C_WARN(&game->log, "we have too many steps in authoritative buffer (%zu). Waiting for state from client.",
                      authoritativeSteps->stepsCount)
            break;
        }

        StepId lookingFor = authoritativeSteps->expectedWriteId;
        int existsContribution = checkEveryoneHasAStepAndDisconnectIfNeeded(connections, lookingFor);
        if (existsContribution == 0) {
            CLOG_C_VERBOSE(&game->log, "authoritative: everyone didn't have step contributions for %08X", lookingFor);
            break;
        }
        if (existsContribution < 0) {
            CLOG_C_SOFT_ERROR(&game->log,"couldn't check if everyone has a step")
            return existsContribution;
        }
        // CLOG_VERBOSE("authoritative: we can advance step %08X. Create a combined step", lookingFor);

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
            CLOG_C_SOFT_ERROR(&game->log,"authoritative: can't have zero participant count")
            return -48;
        }

        int octetsWritten = nbsStepsWrite(authoritativeSteps, lookingFor, composeStepBuffer,
                                          authoritativeStepOctetCount);
        if (octetsWritten < 0) {
            CLOG_C_SOFT_ERROR(&game->log, "authoritative: couldn't write")
            return octetsWritten;
        }
        CLOG_C_VERBOSE(&game->log, "authenticate: we have written %08X", lookingFor);

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

static int handleIncomingSteps(NbdGame* foundGame, NbdParticipantConnections* connections, FldInStream* inStream,
                               NbdParticipantConnection* foundParticipantConnection, StepId* outClientWaitingForStepId,
                               uint64_t* outReceiveMask)
{

    StepId clientWaitingForStepId;
    uint64_t receiveMask;

    int errorCode = nbsPendingStepsInSerializeHeader(inStream, &clientWaitingForStepId, &receiveMask);
    if (errorCode < 0) {
        CLOG_C_SOFT_ERROR(&foundParticipantConnection->log, "client step: couldn't in-serialize pending steps")
        return errorCode;
    }

    CLOG_C_VERBOSE(&foundParticipantConnection->log, "handleIncomingSteps: client %d is awaiting step %08X",
                   foundParticipantConnection->id, clientWaitingForStepId)

    size_t stepsThatFollow;
    StepId firstStepId;
    errorCode = nbsStepsInSerializeHeader(inStream, &firstStepId, &stepsThatFollow);
    if (errorCode < 0) {
        CLOG_C_SOFT_ERROR(&foundParticipantConnection->log, "client step: couldn't in-serialize steps")
        return errorCode;
    }

    CLOG_C_VERBOSE(&foundParticipantConnection->log,
                   "handleIncomingSteps: client %d incoming step %08X, %zu steps follow",
                   foundParticipantConnection->id, firstStepId, stepsThatFollow)

    size_t dropped = nbsStepsDropped(&foundParticipantConnection->steps, firstStepId);
    if (dropped > 0) {
        if (dropped > 60) {
            return -3;
        }
        CLOG_C_WARN(&foundParticipantConnection->log,
                    "client step: dropped %zu steps. expected %04X, but got from %04X to %zX", dropped,
                    foundParticipantConnection->steps.expectedWriteId, firstStepId, firstStepId + stepsThatFollow - 1)
        nbdInsertForcedSteps(foundParticipantConnection, dropped);
    }

    int addedStepsCount = nbsStepsInSerialize(inStream, &foundParticipantConnection->steps, firstStepId,
                                              stepsThatFollow);
    if (addedStepsCount < 0) {
        return addedStepsCount;
    }

    int advanceCount = advanceAuthoritativeAsFarAsWeCan(foundGame, connections);
    if (advanceCount < 0) {
        return advanceCount;
    }

    *outClientWaitingForStepId = clientWaitingForStepId;
    *outReceiveMask = receiveMask;

    return advanceCount;
}

static void showStats(NbdParticipantConnection* foundParticipantConnection)
{
#define DEBUG_COUNT (128)
    char debug[DEBUG_COUNT];

    tc_snprintf(debug, DEBUG_COUNT, "server: conn %d step count in incoming buffer", foundParticipantConnection->id);
    statsIntDebug(&foundParticipantConnection->incomingStepCountInBufferStats, debug, "steps");

    tc_snprintf(debug, DEBUG_COUNT, "server: conn %d steps behind authoritative (latency)", foundParticipantConnection->id);
    statsIntDebug(&foundParticipantConnection->stepsBehindStats, debug, "steps");
}

static void updateStats(NbdParticipantConnection* foundParticipantConnection, NbdGame* foundGame,
                        StepId clientWaitingForStepId)
{
    statsIntAdd(&foundParticipantConnection->incomingStepCountInBufferStats,
                foundParticipantConnection->steps.stepsCount);

    size_t stepsBehindForClient = foundGame->authoritativeSteps.expectedWriteId - clientWaitingForStepId;
    statsIntAdd(&foundParticipantConnection->stepsBehindStats, stepsBehindForClient);
}

static int sendStepRanges(FldOutStream* outStream, NbdParticipantConnection* foundParticipantConnection,
                          NbdGame* foundGame, StepId clientWaitingForStepId, uint64_t receiveMask)
{
#define MAX_RANGES_COUNT (3)
    const int maxStepsCount = 8;
    NbsPendingRange ranges[MAX_RANGES_COUNT + 1];
    int rangeCount = nbsPendingStepsRanges(clientWaitingForStepId, foundGame->authoritativeSteps.expectedWriteId,
                                           receiveMask, ranges, MAX_RANGES_COUNT, maxStepsCount);
    if (rangeCount < 0) {
        CLOG_C_VERBOSE(&foundParticipantConnection->log, "ranges error")
        return rangeCount;
    }

    if (rangeCount == 0) {
        foundParticipantConnection->noRangesToSendCounter++;
        if (foundParticipantConnection->noRangesToSendCounter > 2) {
            int noticeTime = foundParticipantConnection->noRangesToSendCounter % 2;
            if (noticeTime == 0) {
                CLOG_C_NOTICE(&foundParticipantConnection->log, "no ranges to send for %d ticks, suspicious",
                              foundParticipantConnection->noRangesToSendCounter);
            }
        }
    } else {
        foundParticipantConnection->noRangesToSendCounter = 0;
    }
    bool moreDebug = true; // (foundParticipantConnection->id == 0);

    nbsPendingStepsRangesDebugOutput(ranges, "server serialize out initial", rangeCount,
                                     foundParticipantConnection->log);

    if (foundGame->authoritativeSteps.expectedWriteId > foundParticipantConnection->nextAuthoritativeStepIdToSend) {
        // CLOG_INFO("client waiting for %0lX, game authoritative stepId is at %0lX", clientWaitingForStepId,
        //        foundGame->authoritativeSteps.expectedWriteId);

        size_t availableCount = foundGame->authoritativeSteps.expectedWriteId -
                                foundParticipantConnection->nextAuthoritativeStepIdToSend;
        const size_t redundancyStepCount = 5;
        if (availableCount > redundancyStepCount) {
            availableCount = redundancyStepCount;
        }
        ranges[rangeCount].startId = foundParticipantConnection->nextAuthoritativeStepIdToSend;
        ranges[rangeCount].count = availableCount;

        StepId maxStepIdToSend = clientWaitingForStepId + NBS_WINDOW_SIZE - 1;
        if (maxStepIdToSend >= foundGame->authoritativeSteps.expectedWriteId) {
            maxStepIdToSend = foundGame->authoritativeSteps.expectedWriteId;
        }

        if (foundParticipantConnection->nextAuthoritativeStepIdToSend + redundancyStepCount < maxStepIdToSend) {
            foundParticipantConnection->nextAuthoritativeStepIdToSend += redundancyStepCount;
        }
        CLOG_C_VERBOSE(&foundParticipantConnection->log, "add continuation range %08X %zu", ranges[rangeCount].startId,
                    ranges[rangeCount].count);
        rangeCount++;
    } else {
        // CLOG_INFO("no need to force any range. we have %016X and client wants %016X",
        // foundGame->authoritativeSteps.expectedWriteId - 1, clientWaitingForStepId);
    }

    StepId lastReceivedStepFromClient = foundParticipantConnection->steps.expectedWriteId - 1;

    size_t bufferDelta = foundParticipantConnection->steps.stepsCount;
    int authoritativeBufferDelta = (int) foundParticipantConnection->steps.expectedWriteId -
                                   (int) foundGame->authoritativeSteps.expectedWriteId;

    nimbleSerializeServerOutStepHeader(outStream, lastReceivedStepFromClient, bufferDelta, authoritativeBufferDelta);

    if (moreDebug) {
        nbsPendingStepsRangesDebugOutput(ranges, "server serialize out", rangeCount, foundParticipantConnection->log);
    }

    return nbsPendingStepsSerializeOutRanges(outStream, &foundGame->authoritativeSteps, ranges, rangeCount);
}

int nbdReqGameStep(NbdGame* foundGame, NbdParticipantConnection* foundParticipantConnection,
                   NbdParticipantConnections* connections, FldInStream* inStream, FldOutStream* outStream)
{
    StepId clientWaitingForStepId;
    uint64_t receiveMask;


    int errorCode = handleIncomingSteps(foundGame, connections, inStream, foundParticipantConnection,
                                        &clientWaitingForStepId, &receiveMask);
    if (errorCode < 0) {
        CLOG_C_SOFT_ERROR(&foundParticipantConnection->log, "problem handling incoming step:%d", errorCode);
        return errorCode;
    }

    updateStats(foundParticipantConnection, foundGame, clientWaitingForStepId);
    if ((foundParticipantConnection->debugCounter++ % 100) == 0) {
        showStats(foundParticipantConnection);
    }

    return sendStepRanges(outStream, foundParticipantConnection, foundGame, clientWaitingForStepId, receiveMask);
}
