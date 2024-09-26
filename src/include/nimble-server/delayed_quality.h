/*----------------------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved. https://github.com/piot/nimble-server-lib
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------------------*/

#ifndef NIMBLE_SERVER_CONNECTION_QUALITY_DELAYED_H
#define NIMBLE_SERVER_CONNECTION_QUALITY_DELAYED_H

#include <imprint/tagged_allocator.h>
#include <nimble-steps/steps.h>
#include <stdbool.h>

struct NimbleServerConnectionQuality;

typedef struct NimbleServerConnectionQualityDelayed {
    size_t impedingDisconnectCounter;
    Clog log;
} NimbleServerConnectionQualityDelayed;

void nimbleServerConnectionQualityDelayedInit(NimbleServerConnectionQualityDelayed* self, Clog log);
void nimbleServerConnectionQualityDelayedReset(NimbleServerConnectionQualityDelayed* self);
bool nimbleServerConnectionQualityDelayedTick(NimbleServerConnectionQualityDelayed* self,
                                              const struct NimbleServerConnectionQuality* quality);

#endif
