#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void ta_init_22khz(void);
void ta_play_loop(const int8_t* pcm_u8, uint32_t length_bytes);
void ta_set_volume(uint8_t left0_right1, uint8_t level0_7);
void ta_stop(void);
// Call once per frame to advance the streaming pointer (simple looping)
void ta_update_stream_60hz(void);
#ifdef __cplusplus
}
#endif
