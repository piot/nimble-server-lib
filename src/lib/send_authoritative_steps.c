/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/nimble-server-lib
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/

#include "send_authoritative_steps.h"

#include <nimble-serialize/server_out.h>
#include <nimble-server/local_party.h>
#include <nimble-server/transport_connection.h>
#include <nimble-steps-serialize/pending_out_serialize.h>

/// Send authoritative steps to a transport connection using a client provided receiveMask.
/// @param outStream stream to send step ranges to
/// @param transportConnection transport connection that wants the steps
/// @param foundGame the game to send steps from
/// @param clientWaitingForStepId client is waiting for this StepId
/// @return negative on error, otherwise number of ranges sent
ssize_t nimbleServerSendStepRanges(FldOutStream* outStream, NimbleServerTransportConnection* transportConnection,
                                   NimbleServerGame* foundGame, StepId clientWaitingForStepId)
{
    NbsPendingRange range;

    StepId startTickId = clientWaitingForStepId;

    if (startTickId >= foundGame->authoritativeSteps.expectedWriteId) {
        startTickId = foundGame->authoritativeSteps.expectedWriteId - 1;
    }

    if (startTickId < foundGame->authoritativeSteps.expectedReadId) {
        CLOG_C_VERBOSE(&transportConnection->log,
                       "client wants to get authoritative %08X, but we only can provide the earliest %08X", startTickId,
                       foundGame->authoritativeSteps.expectedReadId)
        startTickId = foundGame->authoritativeSteps.expectedReadId;
    }

    // CLOG_INFO("client waiting for %0lX, game authoritative stepId is at %0lX", clientWaitingForStepId,
    //        foundGame->authoritativeSteps.expectedWriteId);

    size_t authStepCountToSend = foundGame->authoritativeSteps.expectedWriteId - startTickId;
    const size_t maxRedundancyStepCount = 20;
    if (authStepCountToSend > maxRedundancyStepCount) {
        authStepCountToSend = maxRedundancyStepCount;
    }
    range.startId = startTickId;
    range.count = authStepCountToSend;

    CLOG_C_VERBOSE(&transportConnection->log, "send auth range %08X-%08zX, %zu", range.startId,
                   range.startId + authStepCountToSend - 1, range.count)

    if (authStepCountToSend == 0) {
        transportConnection->noRangesToSendCounter++;
        if (transportConnection->noRangesToSendCounter > 8) {
            int noticeTime = transportConnection->noRangesToSendCounter % 20;
            if (noticeTime == 0) {
                CLOG_C_NOTICE(&transportConnection->log, "no ranges to send for %d ticks, suspicious",
                              transportConnection->noRangesToSendCounter)
            }
        }
    } else {
        transportConnection->noRangesToSendCounter = 0;
    }

    StepId lastReceivedStepFromClient = 0;
    size_t bufferStepCount = 0;
    int8_t authoritativeTickDelta = 0;

    NimbleServerLocalParty* party = transportConnection->assignedParty;
    if (party != 0) {
        lastReceivedStepFromClient = party->highestReceivedStepId;
        bufferStepCount = party->stepsInBufferCount;
        int64_t tickDelta = (int64_t) party->highestReceivedStepId -
                                   (int64_t) foundGame->authoritativeSteps.expectedWriteId;

        if (tickDelta < -127) {
            authoritativeTickDelta = -128;
        } else if (tickDelta > 127) {
            authoritativeTickDelta = 127;
        } else {
            authoritativeTickDelta = (int8_t) tickDelta;
        }
    }
    CLOG_C_VERBOSE(&transportConnection->log, "send auth header tick_id:%08X tick-delta: %hhd, buffer-size:%zu",
                   lastReceivedStepFromClient, authoritativeTickDelta, bufferStepCount)

    int serializeErr = nimbleSerializeServerOutStepHeader(outStream, lastReceivedStepFromClient, bufferStepCount,
                                                          authoritativeTickDelta, &transportConnection->log);
    if (serializeErr < 0) {
        return serializeErr;
    }

    return nbsPendingStepsSerializeOutRanges(outStream, &foundGame->authoritativeSteps, &range, 1);
}
