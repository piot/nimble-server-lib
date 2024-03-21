/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include "check_for_disconnects.h"
#include <clog/clog.h>
#include <nimble-serialize/server_out.h>
#include <nimble-server/participant.h>
#include <nimble-server/participant_connection.h>
#include <nimble-server/participant_connections.h>
#include <nimble-server/transport_connection.h>

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
    nimbleServerParticipantConnectionDisconnect(connection);

    removeReferencesFromGameParticipants(&connection->participantReferences);

    nimbleServerParticipantConnectionsRemove(connections, connection);
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

        bool isWorking = nimbleServerParticipantConnectionUpdate(connection);
        if (!isWorking) {
            disconnectConnection(connections, connection);
        }
    }
}
