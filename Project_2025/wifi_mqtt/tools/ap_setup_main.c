#include "../../hostap/hostapd/ap_setup.h"

int main(void) 
{
    	if(ap_setup_main() != 0)
	{
		return -1;
	}
    	return 0;
}
