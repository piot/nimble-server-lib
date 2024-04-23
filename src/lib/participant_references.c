/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/nimble-server-lib
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/
#include <nimble-server/participant.h>
#include <nimble-server/participant_references.h>

/// Find a participant from a references collection
/// @param self the references
/// @param participantId id to look for
/// @return Participant pointer or NULL.
struct NimbleServerParticipant* nimbleParticipantReferencesFind(NimbleServerParticipantReferences* self,
                                                                NimbleSerializeParticipantId participantId)
{
    for (size_t i = 0; i < self->participantReferenceCount; ++i) {
        NimbleServerParticipant* participant = self->participantReferences[i];
        if (participant->id == participantId) {
            return participant;
        }
    }

    return 0;
}
