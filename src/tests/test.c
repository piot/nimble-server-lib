/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/nimble-server-lib
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/

#include "utest.h"
#include <imprint/default_setup.h>
#include <nimble-server/local_party.h>
#include <nimble-server/server.h>

UTEST(NimbleSteps, verifyHostMigration)
{
    ImprintDefaultSetup imprintSetup;

    imprintDefaultSetupInit(&imprintSetup, 32 * 1024 * 1024);

    NimbleServer server;

    NimbleServerSetup setup = {.applicationVersion.major = 0,
                               .applicationVersion.minor = 0,
                               .applicationVersion.patch = 0,
                               .memory = &imprintSetup.tagAllocator.info,
                               .blobAllocator = &imprintSetup.slabAllocator.info,
                               .maxConnectionCount = 16,
                               .maxParticipantCount = 16,
                               .maxSingleParticipantStepOctetCount = 20,
                               .maxParticipantCountForEachConnection = 2,
                               .maxWaitingForReconnectTicks = 32,
                               .maxGameStateOctetCount = 32,
                               .callbackObject.self = 0,
                               .now = 0,
                               .targetTickTimeMs = 16,
                               .log.config = &g_clog,
                               .log.constantPrefix = "server"};

    int initErr = nimbleServerInit(&server, setup);

    imprintDefaultSetupDebugOutput(&imprintSetup, "after server init");

    NimbleServerCircularBuffer* freeList = &server.game.participants.freeList;
    ASSERT_GE(0, initErr);

    int reInitErr = nimbleServerReInitWithGame(&server, 0, 0);
    ASSERT_GE(0, reInitErr);
    size_t countBefore = nimbleServerCircularBufferCount(freeList);
    ASSERT_EQ(setup.maxParticipantCount, countBefore); // All participant ids should be free

    NimbleSerializeParticipantId participantIds[] = {0x02, 0x04, 0x01, 0x08};

    size_t testParticipantCount = sizeof(participantIds) / sizeof(participantIds[0]);
    NimbleSerializeLocalPartyInfo localPartyInfo[4] = {
        {.participantCount = 1, .participantIds[0] = participantIds[0]},
        {.participantCount = 1, .participantIds[0] = participantIds[1]},
        {.participantCount = 1, .participantIds[0] = participantIds[2]},
        {.participantCount = 1, .participantIds[0] = participantIds[3]},

    };
    int migrationErr = nimbleServerHostMigration(&server, localPartyInfo, testParticipantCount);

    ASSERT_GE(0, migrationErr);
    size_t countAfterPrepareHostMigration = nimbleServerCircularBufferCount(freeList);
    ASSERT_EQ(setup.maxParticipantCount - testParticipantCount,
              countAfterPrepareHostMigration); // All participant ids should be free

    for (size_t i = 0; i < countAfterPrepareHostMigration; ++i) {
        size_t index = (i + freeList->tail) % NIMBLE_SERVER_CIRCULAR_BUFFER_SIZE;
        CLOG_DEBUG("index %zu data: %hhu", i, freeList->data[index])
    }

    for (size_t i = 0; i < server.localParties.partiesCount; ++i) {
        const NimbleServerLocalParty* party = &server.localParties.parties[i];
    }
}
