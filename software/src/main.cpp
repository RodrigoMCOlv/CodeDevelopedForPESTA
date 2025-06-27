#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "CAN_bridge.h"


/*CAN*/
extern "C" {
#include "rp_agrolib_can.h"
}



/*******************************CAN DEFINES*******************************/
#define CAN0_RX 2           //CAN0 RX PIN
#define CAN0_TX 3           //CAN0 TX PIN


#define CAN1_RX 6           //CAN1 RX PIN
#define CAN1_TX 7           //CAN1 TX PIN


#define BITRATE_CAN 125000  //CAN bitrate
#define MAX_DLC_CAN_MSG 8   //Maximum number of bytes in the CAN message


// Global CAN bus objects
// from rp_agrolib_can.h
extern struct can2040 cbus0;
extern struct can2040 cbus1;


void can2040_cb0(struct can2040 *cd, uint32_t notify, struct can2040_msg *msg){ //The callback function for CAN bus 0

  can_rx_callback(cd, msg->id, msg->dlc, msg->data);
}

void can2040_cb1(struct can2040 *cd, uint32_t notify, struct can2040_msg *msg){ //The callback function for CAN bus 1

  can_rx_callback(cd, msg->id, msg->dlc, msg->data);
}

// ---
//Core 1 Entry Point
//This function will run on **CPU 1**. It initializes CAN bus 1 and then enters an infinite loop to keep the core alive.

void core1_entry() {    //Core 1 handles CAN bus 1

    canbus_setup1(CAN1_RX, CAN1_TX, BITRATE_CAN, can2040_cb1);
    while (1) {
        tight_loop_contents();  //keep core 1 alive
    }
}


int main() {
  stdio_init_all();

  // Launch core1_entry on CPU 1
  multicore_launch_core1(core1_entry);

  /*************************CAN INIT***************************/
  canbus_setup0(CAN0_RX, CAN0_TX, BITRATE_CAN, can2040_cb0);    //Core 0 handles CAN bus 0

  while(1){
    
    sleep_ms(1000);

  }
}