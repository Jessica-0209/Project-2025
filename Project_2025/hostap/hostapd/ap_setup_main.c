#include <stdio.h>
#include "../../wifi_mqtt/hostapd/ap_client.h"
int main(void) {

	if(ap_setup_main() != 0) {
		return -1;
	}

	return 0;
}
