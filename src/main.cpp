#include "bn_core.h"
#include "bn_regular_bg_ptr.h"
#include "bn_regular_bg_items_title.h"
#include "bn_sound.h"
#include "bn_fixed.h"
#include "bn_keypad.h"
#include "bn_sound_items.h"

int main()
{
	bn::core::init();
	bn::regular_bg_ptr bg = bn::regular_bg_items::title.create_bg(0, 0);

	// Single continuous noise: play once, no panning.
	bn::fixed target_volume = bn::fixed(0.8);
	bn::sound::set_master_volume(0); // fade in

	bn::sound_handle noise = bn::sound_items::white_noise.play(bn::fixed(1), bn::fixed(1), bn::fixed(0));

	while(true)
	{
		// Keep a single instance alive; if it ends, restart seamlessly (no panning)
		if(noise.active())
		{
			// no-op
		}
		else
		{
			noise = bn::sound_items::white_noise.play(bn::fixed(1), bn::fixed(1), bn::fixed(0));
		}

		// Fade in master volume towards target
		bn::fixed mv = bn::sound::master_volume();
		if(mv < target_volume)
		{
			mv += bn::fixed(0.004);
			if(mv > target_volume)
			{
				mv = target_volume;
			}
			bn::sound::set_master_volume(mv);
		}

		bn::core::update();
	}
}
