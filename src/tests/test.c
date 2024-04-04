/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/nimble-server-lib
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/
#include "utest.h"
#include <imprint/default_setup.h>
#include <nimble-server/server.h>

UTEST(NimbleSteps, verifyHostMigration)
{
    ImprintDefaultSetup imprintSetup;

    imprintDefaultSetupInit(&imprintSetup, 4 * 1024 * 1024);

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
    ASSERT_GE(0, initErr);
    int reInitErr = nimbleServerReInitWithGame(&server, 0, 0);
    ASSERT_GE(0, reInitErr);

    NimbleSerializeParticipantId participantIds[] = {
        0x42,
        0x10,
        0x18,
        0x08
    };

    int migrationErr = nimbleServerHostMigration(&server, participantIds, sizeof(participantIds)/sizeof(participantIds[0]));
    ASSERT_GE(0, migrationErr);
}
