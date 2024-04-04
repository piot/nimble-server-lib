/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/nimble-server-lib
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/
#include "transport_connection_stats.h"
#include <nimble-server/participant_connection.h>
#include <nimble-server/transport_connection.h>

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

/// Update stats for the transport connection.
/// @param transportConnection transport connection
/// @param foundGame game to update stats for
/// @param clientWaitingForStepId client is waiting for this stepID
void nimbleServerTransportConnectionUpdateStats(NimbleServerTransportConnection* transportConnection,
                                                NimbleServerGame* foundGame, StepId clientWaitingForStepId)
{
    NimbleServerParticipantConnection* foundParticipantConnection = transportConnection->assignedParticipantConnection;
    if (foundParticipantConnection != 0) {
        statsIntAdd(&foundParticipantConnection->incomingStepCountInBufferStats,
                    (int) foundParticipantConnection->steps.stepsCount);
    }

    size_t stepsBehindForClient = foundGame->authoritativeSteps.expectedWriteId - clientWaitingForStepId;
    statsIntAdd(&transportConnection->stepsBehindStats, (int) stepsBehindForClient);

    if ((transportConnection->debugCounter++ % 3000) == 0) {
        showStats(transportConnection);
    }
}
