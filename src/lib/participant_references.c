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
