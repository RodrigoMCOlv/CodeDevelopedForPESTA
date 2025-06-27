#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "CAN_bridge.h"


/*CAN*/
extern "C" {
#include "rp_agrolib_can.h"
}

#define DEBUG 0


/*******************************CAN DEFINES*******************************/
#define CAN0_RX 2
#define CAN0_TX 3


#define CAN1_RX 6
#define CAN1_TX 7


#define BITRATE_CAN 125000
#define MAX_DLC_CAN_MSG 8 //Maximum number of bytes in the CAN message


// Global CAN bus objects (if not already global in rp_agrolib_can.h)
// You might need to declare these if they are not accessible globally
// from rp_agrolib_can.h or CAN_bridge.h
extern struct can2040 cbus0;
extern struct can2040 cbus1;


void can2040_cb0(struct can2040 *cd, uint32_t notify, struct can2040_msg *msg){

  if (DEBUG){
    printf("Callback CAN 0, ID: %d", msg->id);
    //printf(" Data: %d %d %d %d %d %d %d %d", msg->data[0], msg->data[1], msg->data[2], msg->data[3], msg->data[4], msg->data[5], msg->data[6], msg->data[7]);
    printf("\n");
  }

  can_rx_callback(cd, msg->id, msg->dlc, msg->data);
}

void can2040_cb1(struct can2040 *cd, uint32_t notify, struct can2040_msg *msg){

  if (DEBUG){
    printf("Message CAN 1, ID: %d", msg->id);
    //printf(" Data: %d %d %d %d %d %d %d %d", msg->data[0], msg->data[1], msg->data[2], msg->data[3], msg->data[4], msg->data[5], msg->data[6], msg->data[7]);
    printf("\n");
  }

  can_rx_callback(cd, msg->id, msg->dlc, msg->data);
}

// ---
//Core 1 Entry Point
//This function will run on **CPU 1**. It initializes CAN bus 1 and then enters an infinite loop to keep the core alive.

void core1_entry() {
    canbus_setup1(CAN1_RX, CAN1_TX, BITRATE_CAN, can2040_cb1);
    while (1) {
        // Core 1 will handle CAN1 interrupts and callbacks
        tight_loop_contents(); // Or other tasks specific to Core 1
    }
}


int main() {
  stdio_init_all();

  // Launch core1_entry on CPU 1
  multicore_launch_core1(core1_entry);

  /*************************CAN INIT***************************/
  canbus_setup0(CAN0_RX, CAN0_TX, BITRATE_CAN, can2040_cb0);

  while(1){
    
    sleep_ms(1000);

  }
}