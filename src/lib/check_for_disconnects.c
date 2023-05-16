/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include "check_for_disconnects.h"
#include <nimble-serialize/server_out.h>
#include <nimble-server/participant_connection.h>
#include <nimble-server/participant_connections.h>
#include <nimble-server/transport_connection.h>

#define NBD_LOGGING 1

static void disconnectConnection(NimbleServerParticipantConnections* connections,
                                 NimbleServerParticipantConnection* connection)
{
    connection->transportConnection->isUsed = false;
    nbdParticipantConnectionsRemove(connections, connection);
}

void nimbleServerCheckForDisconnections(NimbleServerParticipantConnections* connections)
{
    for (size_t i = 0; i < connections->capacityCount; ++i) {
        NimbleServerParticipantConnection* connection = &connections->connections[i];
        if (!connection->isUsed) {
            continue;
        }
        if (connection->forcedStepInRowCounter > 120U) {
            disconnectConnection(connections, connection);
        } else if (connection->forcedStepInRowCounter > 4U) {
            if (connection->state == NimbleServerParticipantConnectionStateNormal) {
                connection->state = NimbleServerParticipantConnectionStateImpendingDisconnect;
                connection->impedingDisconnectCounter++;
                if (connection->impedingDisconnectCounter >= 3) {
                    disconnectConnection(connections, connection);
                }
            }
        }
        if (connection->impedingDisconnectCounter > 0 && connection->providedStepsInARow > 62 * 60U) {
            connection->impedingDisconnectCounter = 0;
            connection->state = NimbleServerParticipantConnectionStateNormal;
        }
    }
}
