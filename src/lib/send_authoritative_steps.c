/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include "send_authoritative_steps.h"

#include <nimble-serialize/server_out.h>
#include <nimble-server/participant_connection.h>
#include <nimble-server/transport_connection.h>
#include <nimble-steps-serialize/pending_out_serialize.h>
#include <nimble-steps/pending_steps.h>

int nimbleServerSendStepRanges(FldOutStream* outStream, NimbleServerTransportConnection* transportConnection,
                               NimbleServerGame* foundGame, StepId clientWaitingForStepId, uint64_t receiveMask,
                               uint16_t receivedTimeFromClient)
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

        NimbleServerParticipantConnection* foundParticipantConnection = transportConnection
                                                                            ->assignedParticipantConnection;
        if (foundParticipantConnection != 0) {
            lastReceivedStepFromClient = foundParticipantConnection->steps.expectedWriteId - 1;
            bufferDelta = foundParticipantConnection->steps.stepsCount;
            authoritativeBufferDelta = (int) foundParticipantConnection->steps.expectedWriteId -
                                       (int) foundGame->authoritativeSteps.expectedWriteId;
        }
    }

    nimbleSerializeServerOutStepHeader(outStream, lastReceivedStepFromClient, bufferDelta, authoritativeBufferDelta,
                                       receivedTimeFromClient);

    if (moreDebug) {
        nbsPendingStepsRangesDebugOutput(ranges, "server serialize out", rangeCount, transportConnection->log);
    }

    return nbsPendingStepsSerializeOutRanges(outStream, &foundGame->authoritativeSteps, ranges, rangeCount);
}
