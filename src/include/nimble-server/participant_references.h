#ifndef NIMBLE_SERVER_PARTICIPANT_REFERENCES_H
#define NIMBLE_SERVER_PARTICIPANT_REFERENCES_H

#include <nimble-serialize/types.h>

typedef struct NimbleServerParticipantReferences {
    size_t participantReferenceCount;
    struct NimbleServerParticipant* participantReferences[NIMBLE_SERIALIZE_MAX_LOCAL_PLAYERS];
    struct NimbleServerParticipants* gameParticipants;
} NimbleServerParticipantReferences;

struct NimbleServerParticipant* nimbleParticipantReferencesFind(NimbleServerParticipantReferences* self, NimbleSerializeParticipantId participantId);

#endif // NIMBLE_SERVER_PARTICIPANT_REFERENCES_H
