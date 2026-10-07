#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <net/if.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include <linux/can.h>
#include <linux/can/raw.h>

#include <ncurses.h>

#include "PowerDistributionPanel.hpp"

int main(void) {
	int s;
	int nbytes;
	struct sockaddr_can addr;
	struct can_frame frame;
	struct ifreq ifr;

	initscr();
	curs_set(0);
	clear();

	const char *ifname = "can0";

	if((s = socket(PF_CAN, SOCK_RAW, CAN_RAW)) < 0) {
		perror("Error while opening socket");
		return -1;
	}

	strcpy(ifr.ifr_name, ifname);
	ioctl(s, SIOCGIFINDEX, &ifr);
	
	addr.can_family  = AF_CAN;
	addr.can_ifindex = ifr.ifr_ifindex;

	printf("%s at index %d\n", ifname, ifr.ifr_ifindex);

	if(bind(s, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		perror("Error in socket bind");
		return -2;
	}


	PowerDistributionPanel pdp;
	while(1){
		nbytes = read(s, &frame, sizeof(struct can_frame));
		if(nbytes==-1) continue; 

		move(0,0);
		printw("bytes read: %d", nbytes);
		printw("   address: %X", frame.can_id);
		printw("   payload byte count: %X", frame.can_dlc);

		
		pdp.parseFrame(frame);

		move(3,3);
		printw("Voltage: %f ",pdp.getVoltage());		
		for(int index=0;index<15;index++){
			move(4+index,3);
			printw("Current %d: %f %f %f                       ",index,
				pdp.getCurrent(index),
				pdp.getCurrentB(index),
				pdp.getCurrentC(index)
				);
		}

		refresh();
        }
	
	return 0;
}
