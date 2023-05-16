/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include "transport_connection_stats.h"
#include <nimble-server/transport_connection.h>
#include <nimble-server/participant_connection.h>

static void showStats(NimbleServerTransportConnection* transportConnection)
{
#define DEBUG_COUNT (128)
    char debug[DEBUG_COUNT];

    NimbleServerParticipantConnection* foundParticipantConnection = transportConnection->assignedParticipantConnection;

    if (foundParticipantConnection) {
        tc_snprintf(debug, DEBUG_COUNT, "server: conn %d step count in incoming buffer",
                    foundParticipantConnection->id);
        statsIntDebug(&foundParticipantConnection->incomingStepCountInBufferStats, &transportConnection->log, debug,
                      "steps");
    }

    tc_snprintf(debug, DEBUG_COUNT, "server: conn %d steps behind authoritative (latency)",
                transportConnection->transportConnectionId);
    statsIntDebug(&transportConnection->stepsBehindStats, &transportConnection->log, debug, "steps");
}

void nimbleServerTransportConnectionUpdateStats(NimbleServerTransportConnection* transportConnection,
                                                NimbleServerGame* foundGame, StepId clientWaitingForStepId)
{
    NimbleServerParticipantConnection* foundParticipantConnection = transportConnection->assignedParticipantConnection;
    if (foundParticipantConnection != 0) {
        statsIntAdd(&foundParticipantConnection->incomingStepCountInBufferStats,
                    foundParticipantConnection->steps.stepsCount);
    }

    size_t stepsBehindForClient = foundGame->authoritativeSteps.expectedWriteId - clientWaitingForStepId;
    statsIntAdd(&transportConnection->stepsBehindStats, stepsBehindForClient);

    if ((transportConnection->debugCounter++ % 3000) == 0) {
        showStats(transportConnection);
    }
}
