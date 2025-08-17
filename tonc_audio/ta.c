#include <stdint.h>
#include "tonc.h"

static const int8_t* _loop_data = 0;
static uint32_t _loop_len = 0;
static uint32_t _read_pos = 0;

void ta_init_22khz(void)
{
	// Enable sound, DirectSound A to both L/R, timer 0, reset FIFOs
	// Turn off DMG channels, set master volume to max
	REG_SOUNDCNT_L = SDMG_BUILD(0, 0, 7, 7);
	// Direct Sound A: 100% volume, output to both L and R, Timer 0, reset FIFO
	REG_SOUNDCNT_H = SDS_DMG100 | SDS_A100 | SDS_AR | SDS_AL | SDS_ATMR0 | SDS_ARESET;
	REG_SOUNDCNT_X = SSTAT_ENABLE;

	// Timer0 for ~22.05kHz
	// Formula from Deku: Timer = 65536 - (16777216 / sample_rate)
	REG_TM0CNT = 0;
	REG_TM0D = 65536 - (16777216 / 22050);
	REG_TM0CNT = TM_ENABLE;
}

void ta_play_loop(const int8_t* pcm_u8, uint32_t length_bytes)
{
	_loop_data = pcm_u8;
	_loop_len = length_bytes;

	// Stop any existing DMA
	REG_DMA1CNT = 0;
	
	// Set up DMA1 for Direct Sound channel A
	REG_DMA1SAD = (u32)_loop_data;
	REG_DMA1DAD = (u32)&REG_FIFO_A;
	// DMA: 32-bit transfers, repeat, FIFO mode (special timing), dest fixed
	REG_DMA1CNT = DMA_DST_FIXED | DMA_REPEAT | DMA_32 | DMA_AT_FIFO | DMA_ENABLE;
}

void ta_update_stream_60hz(void)
{
	// For simple looping, restart DMA when it reaches the end
	if(!_loop_data || !_loop_len)
		return;
	
	// Check if we need to loop (simplified approach)
	// In a real implementation, you'd use double buffering like Deku
	_read_pos += (22050 / 60) * 4; // bytes consumed per frame
	if(_read_pos >= _loop_len) {
		_read_pos = 0;
		// Restart DMA from beginning
		REG_DMA1CNT = 0;
		REG_DMA1SAD = (u32)_loop_data;
		REG_DMA1DAD = (u32)&REG_FIFO_A;
		REG_DMA1CNT = DMA_DST_FIXED | DMA_REPEAT | DMA_32 | DMA_AT_FIFO | DMA_ENABLE;
	}
}

void ta_set_volume(uint8_t lr, uint8_t level)
{
	// set DirectSound A volume (simple): 50%/100% flags are coarse; use 100%
	(void)lr; (void)level;
}

void ta_stop(void)
{
	REG_DMA1CNT = 0;
}
