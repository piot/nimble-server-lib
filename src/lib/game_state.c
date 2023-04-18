/*---------------------------------------------------------------------------------------------
 *  Copyright (c) Peter Bjorklund. All rights reserved.
 *  Licensed under the MIT License. See LICENSE in the project root for license information.
 *--------------------------------------------------------------------------------------------*/
#include <imprint/allocator.h>
#include <nimble-server/game_state.h>

void nbdGameStateInit(NbdGameState* self, ImprintAllocator* allocator, size_t octetCount)
{
    self->state = IMPRINT_ALLOC_TYPE_COUNT(allocator, uint8_t, octetCount);
    self->capacity = octetCount;
    self->stepId = 0;
    self->octetCount = 0;
}

void nbdGameStateDestroy(NbdGameState* self)
{
}
