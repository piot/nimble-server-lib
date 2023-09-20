/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#ifndef NIMBLE_SERVER_UPDATE_QUALITY_H
#define NIMBLE_SERVER_UPDATE_QUALITY_H

#include <monotonic-time/monotonic_time.h>
#include <stats/stats.h>

typedef enum NimbleServerUpdateQualityState {
    NimbleServerUpdateQualityStateWorking,
    NimbleServerUpdateQualityStateFailedTickTime,
    NimbleServerUpdateQualityStateFailedAverageTickTime
} NimbleServerUpdateQualityState;

typedef struct NimbleServerUpdateQuality {
    MonotonicTimeMs lastTimeMs;
    StatsInt measuredDeltaTimeMsStat;
    size_t averageTickTimeFailedInARow;
    size_t deltaTickTimeFailedInARow;
    size_t targetTimeMs;
    NimbleServerUpdateQualityState state;
} NimbleServerUpdateQuality;

void nimbleServerUpdateQualityInit(NimbleServerUpdateQuality* self, size_t targetTimeMs);
void nimbleServerUpdateQualityReInit(NimbleServerUpdateQuality* self);
int nimbleServerUpdateQualityTick(NimbleServerUpdateQuality* self);

#endif
