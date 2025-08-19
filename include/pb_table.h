#ifndef PB_TABLE_H
#define PB_TABLE_H

#include <stdint.h>

#define PB_AZ_COUNT 8
#define PB_FB_COUNT 2
#define PB_LEN 3371

extern const int8_t* const pb_table_L[PB_AZ_COUNT*PB_FB_COUNT];
extern const int8_t* const pb_table_R[PB_AZ_COUNT*PB_FB_COUNT];
#endif
