#include "bn_core.h"
#include "bn_regular_bg_ptr.h"
#include "bn_regular_bg_items_title.h"

int main()
{
	bn::core::init();
	bn::regular_bg_ptr bg = bn::regular_bg_items::title.create_bg(0, 0);
	while(true)
	{
		bn::core::update();
	}
}
