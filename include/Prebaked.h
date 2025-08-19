#ifndef PREBAKED_H
#define PREBAKED_H

#include "tonc.h"

void Prebaked_init(const s8* baseData, u32 lengthSamples, u32 loopStart, u32 loopLength, u32 baseHz);
void Prebaked_enable(bool enable);
void Prebaked_update_by_position(int x, int y, u32 volume0to64);
// Will later switch between fixed buckets; for now keep single centered pair
void Prebaked_stop(void);

#endif


