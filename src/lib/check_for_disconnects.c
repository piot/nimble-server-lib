/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include "check_for_disconnects.h"
#include <nimble-serialize/server_out.h>
#include <nimble-server/participant_connection.h>
#include <nimble-server/participant_connections.h>
#include <nimble-server/transport_connection.h>
#include <nimble-server/participant.h>

static void removeReferencesFromGameParticipants(NimbleServerParticipantReferences* participantReferences)
{
    NimbleServerParticipants* gameParticipants = participantReferences->gameParticipants;
    for (size_t i = 0; i < participantReferences->participantReferenceCount; ++i) {
        NimbleServerParticipant* participant = participantReferences->participantReferences[i];
        CLOG_ASSERT(gameParticipants->participantCount > 0,
                    "participant count is wrong when removing game participants")
        gameParticipants->participantCount--;

        participant->isUsed = false;
        participant->localIndex = 0;
    }
}

static void disconnectConnection(NimbleServerParticipantConnections* connections,
                                 NimbleServerParticipantConnection* connection)
{
    CLOG_C_DEBUG(&connection->log, "removed connection due to disconnect")
    connection->transportConnection->isUsed = false;
    connection->state = NimbleServerParticipantConnectionStateDisconnected;

    removeReferencesFromGameParticipants(&connection->participantReferences);

    nimbleServerParticipantConnectionsRemove(connections, connection);
}

static void setConnectionInWaitingForReconnect(NimbleServerParticipantConnection* connection)
{
    connection->state = NimbleServerParticipantConnectionStateWaitingForReconnect;
    connection->waitingForReconnectTimer = 0;
}



static void checkWaitingForReconnect(NimbleServerParticipantConnections* connections,
                                     NimbleServerParticipantConnection* connection)
{
    connection->waitingForReconnectTimer++;
    if (connection->waitingForReconnectTimer < connection->waitingForReconnectMaxTimer) {
        return;
    }

    CLOG_C_DEBUG(&connection->log, "gave up on reconnect for connection, waited %zu ticks. disconnecting it now",
                 connection->waitingForReconnectTimer)

    disconnectConnection(connections, connection);
}

static void checkDisconnectInNormal(NimbleServerParticipantConnection* self)
{
    bool shouldDisconnect = nimbleServerConnectionQualityCheckIfShouldDisconnect(&self->quality);
    if (shouldDisconnect) {
        setConnectionInWaitingForReconnect(self);
    }
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
                checkDisconnectInNormal(connection);
                break;
            case NimbleServerParticipantConnectionStateWaitingForReconnect:
                checkWaitingForReconnect(connections, connection);
                break;
            case NimbleServerParticipantConnectionStateDisconnected:
                break;
        }
    }
}
