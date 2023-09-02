/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include "check_for_disconnects.h"
#include <nimble-serialize/server_out.h>
#include <nimble-server/participant_connection.h>
#include <nimble-server/participant_connections.h>
#include <nimble-server/transport_connection.h>

static void disconnectConnection(NimbleServerParticipantConnections* connections,
                                 NimbleServerParticipantConnection* connection)
{
    connection->transportConnection->isUsed = false;
    connection->state = NimbleServerParticipantConnectionStateDisconnected;
    nimbleServerParticipantConnectionsRemove(connections, connection);
}

static void setConnectionInWaitingForReconnect(NimbleServerParticipantConnection* connection)
{
    connection->state = NimbleServerParticipantConnectionStateWaitingForReconnect;
    connection->waitingForReconnectTimer = 0;
}

static void checkDisconnectInNormal(NimbleServerParticipantConnection* connection, Clog* log)
{
    (void) log;
    size_t forcedStepInRowCounterThreshold = 120U;
    if (!blobStreamLogicOutIsComplete(&connection->transportConnection->blobStreamLogicOut)) {
        forcedStepInRowCounterThreshold = 280U;
    }

    if (connection->forcedStepInRowCounter > forcedStepInRowCounterThreshold) {
        CLOG_C_DEBUG(log, "disconnect connection %d due to not providing steps for a long time", connection->id)
        setConnectionInWaitingForReconnect(connection);
    } else if (connection->forcedStepInRowCounter > 4U) {
        if (connection->state == NimbleServerParticipantConnectionStateNormal) {
            connection->state = NimbleServerParticipantConnectionStateImpendingDisconnect;
            connection->impedingDisconnectCounter++;
            if (connection->impedingDisconnectCounter >= 3) {
                CLOG_C_DEBUG(log, "disconnect connection %d due to multiple occurrences of not providing steps",
                             connection->id)
                setConnectionInWaitingForReconnect(connection);
            }
        }
    }
}

static void checkDisconnectWhileImpendingDisconnect(NimbleServerParticipantConnection* connection)
{
    if (connection->impedingDisconnectCounter > 0 && connection->providedStepsInARow > 62 * 60U) {
        connection->impedingDisconnectCounter = 0;
        connection->state = NimbleServerParticipantConnectionStateNormal;
    }
}

static void checkWaitingForReconnect(NimbleServerParticipantConnections* connections,
                                     NimbleServerParticipantConnection* connection)
{
    connection->waitingForReconnectTimer++;
    if (connection->waitingForReconnectTimer < connection->waitingForReconnectMaxTimer) {
        return;
    }

    disconnectConnection(connections, connection);
}

/// Check all connections and disconnect those that have low network quality.
/// @param connections transport connections to check
void nimbleServerCheckForDisconnections(NimbleServerParticipantConnections* connections)
{
    for (size_t i = 0; i < connections->capacityCount; ++i) {
        NimbleServerParticipantConnection* connection = &connections->connections[i];
        if (!connection->isUsed || connection->transportConnection->phase == NbTransportConnectionPhaseDisconnected) {
            continue;
        }

        switch (connection->state) {
            case NimbleServerParticipantConnectionStateNormal:
                checkDisconnectInNormal(connection, &connections->log);
                break;
            case NimbleServerParticipantConnectionStateImpendingDisconnect:
                checkDisconnectWhileImpendingDisconnect(connection);
                break;
            case NimbleServerParticipantConnectionStateWaitingForReconnect:
                checkWaitingForReconnect(connections, connection);
                break;
            case NimbleServerParticipantConnectionStateDisconnected:
                break;
        }
    }
}
