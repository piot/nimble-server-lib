/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include "authoritative_steps.h"
#include "incoming_predicted_steps.h"
#include "nimble-server/server.h"
#include "send_authoritative_steps.h"
#include "transport_connection_stats.h"
#include <flood/in_stream.h>
#include <inttypes.h>
#include <nimble-server/participant_connection.h>
#include <nimble-server/req_step.h>

static int discardAuthoritativeStepsIfBufferGettingFull(NimbleServerGame* foundGame)
{
    size_t authoritativeStepCount = foundGame->authoritativeSteps.stepsCount;
    size_t maxCapacity = NBS_WINDOW_SIZE / 3;

    if (authoritativeStepCount > maxCapacity) {
        size_t authoritativeToDrop = authoritativeStepCount - maxCapacity;
        CLOG_C_VERBOSE(&foundGame->log, "discarding %zu old authoritative steps due to buffer getting full",
                       authoritativeToDrop)
        int err = nbsStepsDiscardCount(&foundGame->authoritativeSteps, authoritativeToDrop);
        if (err < 0) {
            return err;
        }
        CLOG_C_VERBOSE(&foundGame->log, "oldest step after discard is %04X with count %zu",
                       foundGame->authoritativeSteps.expectedReadId, foundGame->authoritativeSteps.stepsCount)
    }

    return 0;
}

static int readIncomingStepsAndCreateAuthoritativeSteps(NimbleServerGame* foundGame,
                                                        NimbleServerParticipantConnections* connections,
                                                        FldInStream* inStream,
                                                        NimbleServerTransportConnection* transportConnection,
                                                        StatsIntPerSecond* authoritativeStepsPerSecondStat,
                                                        StepId* outClientWaitingForStepId, uint64_t* outReceiveMask)
{
    int discardErr = discardAuthoritativeStepsIfBufferGettingFull(foundGame);
    if (discardErr < 0) {
        return discardErr;
    }

    int receivedCount = nimbleServerHandleIncomingSteps(foundGame, inStream, transportConnection,
                                                        outClientWaitingForStepId, outReceiveMask);
    if (receivedCount < 0) {
        return receivedCount;
    }

    int advanceCount = 0;
    if (!foundGame->debugIsFrozen) {
        advanceCount = nimbleServerComposeAuthoritativeSteps(foundGame, connections);
        if (advanceCount < 0) {
            return advanceCount;
        }

        statsIntPerSecondAdd(authoritativeStepsPerSecondStat, advanceCount);
    }

    return advanceCount;
}

/// Handles a request from the client to insert predicted inputs into the authoritative step buffer
/// It will respond with sending authoritative steps that the client requires.
/// @param foundGame game
/// @param transportConnection transport connection that provides the steps
/// @param authoritativeStepsPerSecondStat stats to update
/// @param connections participant connections collection
/// @param inStream stream to read from
/// @param outStream out stream for reply
/// @return negative on error
int nimbleServerReqGameStep(NimbleServerGame* foundGame, NimbleServerTransportConnection* transportConnection,
                            StatsIntPerSecond* authoritativeStepsPerSecondStat,
                            NimbleServerParticipantConnections* connections, FldInStream* inStream,
                            FldOutStream* outStream)
{
    StepId clientWaitingForStepId;
    uint64_t receiveMask;

    int errorCode = readIncomingStepsAndCreateAuthoritativeSteps(foundGame, connections, inStream, transportConnection,
                                                                 authoritativeStepsPerSecondStat,
                                                                 &clientWaitingForStepId, &receiveMask);
    if (errorCode < 0) {
        if (!nimbleServerIsErrorExternal(errorCode)) {
            CLOG_C_SOFT_ERROR(&transportConnection->log, "problem handling incoming step:%d", errorCode)
        }
        return errorCode;
    }

    nimbleServerTransportConnectionUpdateStats(transportConnection, foundGame, clientWaitingForStepId);

    return (int) nimbleServerSendStepRanges(outStream, transportConnection, foundGame, clientWaitingForStepId,
                                            receiveMask);
}
