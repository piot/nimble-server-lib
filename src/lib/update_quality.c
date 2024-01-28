/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <nimble-server/update_quality.h>
#include <clog/clog.h>

void nimbleServerUpdateQualityInit(NimbleServerUpdateQuality* self, size_t targetTimeMs)
{
    self->targetTimeMs = targetTimeMs;
    nimbleServerUpdateQualityReInit(self);
}

void nimbleServerUpdateQualityReInit(NimbleServerUpdateQuality* self)
{
    statsIntInit(&self->measuredDeltaTimeMsStat, 10);
    self->deltaTickTimeFailedInARow = 0;
    self->averageTickTimeFailedInARow = 0;
    self->lastTimeMs = monotonicTimeMsNow();
    self->state = NimbleServerUpdateQualityStateWorking;
}

int nimbleServerUpdateQualityTick(NimbleServerUpdateQuality* self)
{
    if (self->state != NimbleServerUpdateQualityStateWorking) {
        return -1;
    }

    MonotonicTimeMs now = monotonicTimeMsNow();

    CLOG_ASSERT(now >= self->lastTimeMs, "monotonic time is going backwards")

    size_t delta = (size_t) (now - self->lastTimeMs);

    statsIntAdd(&self->measuredDeltaTimeMsStat, (int)delta);

    self->lastTimeMs = now;

    if (delta > self->targetTimeMs) {
        self->deltaTickTimeFailedInARow++;
    } else {
        self->deltaTickTimeFailedInARow = 0;
    }

    if (self->measuredDeltaTimeMsStat.avgIsSet) {
        if (self->measuredDeltaTimeMsStat.avg > (int)self->targetTimeMs) {
            self->averageTickTimeFailedInARow++;
        } else {
            self->averageTickTimeFailedInARow = 0;
        }
    }

    const size_t FailedTickThreshold = 60;
    const size_t AverageThreshold = 50;

    if (self->deltaTickTimeFailedInARow > FailedTickThreshold) {
        self->state = NimbleServerUpdateQualityStateFailedTickTime;
        CLOG_NOTICE("failed to update host with a stable tick rate. stopping server.")
        return -1;
    } else if (self->averageTickTimeFailedInARow > AverageThreshold) {
        self->state = NimbleServerUpdateQualityStateFailedAverageTickTime;
        CLOG_NOTICE("failed to update host with a stable average tick rate. stopping server.")
        return -1;
    }

    //CLOG_VERBOSE("quality : tickFailed: %zu, avgFailed: %zu, avg: %d delta: %zu", self->deltaTickTimeFailedInARow, self->averageTickTimeFailedInARow, self->measuredDeltaTimeMsStat.avg, delta)

    return 0;
}
