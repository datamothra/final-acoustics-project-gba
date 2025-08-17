#include "bn_core.h"
#include "bn_regular_bg_ptr.h"
#include "bn_regular_bg_items_title.h"
#include "bn_keypad.h"
#include "tonc.h"

int main()
{
	bn::core::init();
	bn::regular_bg_ptr bg = bn::regular_bg_items::title.create_bg(0, 0);

	// Enable sound system
	REG_SOUNDCNT_X = SSTAT_ENABLE;
	
	// Enable noise channel (channel 4) output to both L and R, max volume
	REG_SOUNDCNT_L = SDMG_BUILD(0, 0, 7, 7) | (1 << 15) | (1 << 11); // Noise to L and R
	
	// Set DMG volume to 100%
	REG_SOUNDCNT_H = SDS_DMG100;

	while(true)
	{
		// Trigger noise on B press
		if(bn::keypad::b_pressed())
		{
			// Configure noise channel
			// Envelope: initial volume 15, decay, step time 3
			REG_SOUND4CNT_L = SSQR_ENV_BUILD(15, 1, 3);
			
			// Frequency: 7-bit mode (white noise), divider 0, shift 2
			// SFREQ_RESET triggers the sound
			REG_SOUND4CNT_H = SFREQ_RESET | (0 << 3) | (2 << 4) | (0 << 7);
		}
		
		bn::core::update();
	}
}