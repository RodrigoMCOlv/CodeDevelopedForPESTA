#ifndef CAN_BRIDGE_H
#define CAN_BRIDGE_H
#include <stdio.h>
#include <string.h>

extern "C" {
    #include "rp_agrolib_can.h"
}

// Define IDs for control and feedback messages
#define CONTROL_ID      0x700
#define FEEDBACK_ID     0x701


// Define interface IDs for clarity
#define CAN_IFACE0 0
#define CAN_IFACE1 1

#define CAN_COMPUTER_IFACE  0
#define CAN_COMPUTER_BUS    cbus0 

#define RECENT_MESSAGES_BUFFER_SIZE 5 // Store 5 recent messages for echo checking

// Struct to store transmitted message details for echo checking
struct transmitted_msg_info {
    uint32_t id;
    uint8_t dlc;
    uint8_t data[8];
};



// Bi directional modes
#define WHITELIST_MODE              0x01    //Only allows the ID's on the list through
#define BLACKLIST_MODE              0x02    //Only allows the ID's off the list through
#define PASSIVE_MODE                0x03    //Simple bridge, no filter is apllied
#define ONE_WAY_ZERO_TO_ONE_MODE    0x04
#define ONE_WAY_ONE_TO_ZERO_MODE    0x05
#define BI_EXCEPT_OW_0_TO_1_MODE    0x06    //Allow all ID's to go both ways except the one added to the list that may only go from 0 to 1
#define BI_EXCEPT_OW_1_TO_0_MODE    0x07    //Allow all ID's to go both ways except the one added to the list that may only go from 1 to 0



// One-way list-based modes
#define OW_1_TO_0_BLACKLIST_MODE    0x08 // Bridge CAN1 to CAN0 if ID NOT in blacklist
#define OW_1_TO_0_WHITELIST_MODE    0x09 // Bridge CAN1 to CAN0 if ID IS in whitelist
#define OW_0_TO_1_BLACKLIST_MODE    0x0A // Bridge CAN0 to CAN1 if ID NOT in blacklist
#define OW_0_TO_1_WHITELIST_MODE    0x0B // Bridge CAN0 to CAN1 if ID IS in whitelist

// One-way modes with exceptions for reverse traffic
#define OW_1_TO_0_EXCEPT_MODE       0x0C // Bridge CAN1 to CAN0 by default; bridge CAN0 to CAN1 if ID IS in exception_list
#define OW_0_TO_1_EXCEPT_MODE       0x0D // Bridge CAN0 to CAN1 by default; bridge CAN1 to CAN0 if ID IS in exception_list



// Commands
#define SET_MODE                    0x01    //Used to set the given mode
#define ADD_ID                      0x02    //Used to add an ID to the list
#define REMOVE_ID                   0x03    //Used to remove an ID from the list
#define CLEAR_LIST                  0x04    //Used to clear the list
#define SET_MODE_AND_CLEAR          0x05    //Used to clear the list right away and set the mode
#define SET_MODE_ADD_ID             0x06    //Used to add an ID to the list right away and set the mode

#define FEEDBACK_BRIDGE             0xFC    //To feedback to the computer
#define FEEDBACK_COMPUTER           0xFD    //To feedback to the computer
#define TURNON                      0xFE    //Turn On the bridge
#define TURNOFF                     0xFF    //Turn Off the bridge

#define ERROR_IN_MESSAGE            0xFE    //When checksum fails send back this code to COMPUTER_ID with bit 0 as the error command
#define CONFIRM                     0xFF    //Used in Turn On and Turn Off to guarantee it was not an accident
#define MAX_LIST_SIZE               20


typedef enum {
    FILTER_MODE_PASSIVE,
    FILTER_MODE_WHITELIST,
    FILTER_MODE_BLACKLIST,
    FILTER_MODE_ZERO_TO_ONE,    
    FILTER_MODE_ONE_TO_ZERO,    
    FILTER_MODE_ONE_WAY_1_TO_0_BLACKLIST,
    FILTER_MODE_ONE_WAY_1_TO_0_WHITELIST,
    FILTER_MODE_ONE_WAY_0_TO_1_BLACKLIST,
    FILTER_MODE_ONE_WAY_0_TO_1_WHITELIST,
    FILTER_MODE_ONE_WAY_1_TO_0_EXCEPT,
    FILTER_MODE_ONE_WAY_0_TO_1_EXCEPT,
    FILTER_MODE_BIDIRECTIONAL_EXCEPT_OW_1_TO_0,
    FILTER_MODE_BIDIRECTIONAL_EXCEPT_OW_0_TO_1,

} FilterMode_t;

static uint32_t whitelist_ids[MAX_LIST_SIZE];
static uint8_t whitelist_count = 0;

static uint32_t blacklist_ids[MAX_LIST_SIZE];
static uint8_t blacklist_count = 0;

static uint32_t exception_ids[MAX_LIST_SIZE]; 
static uint8_t exception_count = 0;

static uint32_t one_way_restricted_ids[MAX_LIST_SIZE];
static uint8_t one_way_restricted_count = 0;


static FilterMode_t current_filter_state = FILTER_MODE_PASSIVE;

static bool bridge_enabled = true;

//The lists 
#define MAX_LIST_SIZE               10




//END OF FILTER DEFINES

/*Standard bridge funcions*/
void add_recent_tx_message(const struct can2040_msg *msg, uint8_t dlc, uint8_t tx_interface_id);
bool is_echo(const struct can2040_msg *received_msg, uint8_t received_dlc, uint8_t rx_interface_id);
void bridge_transmit(struct can2040 *target_bus, struct can2040_msg *msg, uint8_t dlc, uint8_t tx_interface_id);
void can_rx_callback(struct can2040 *can_instance_ptr, uint32_t id, uint8_t dlc, uint8_t *data_payload);


/*Bridge filtering functions*/
static uint32_t get_id_from_data(const uint8_t *data);
static int8_t find_id_in_list(uint32_t id, const uint32_t *list, uint8_t count);
static bool add_id_to_list(uint32_t id, uint32_t *list, uint8_t *count, uint8_t max_size);
static bool remove_id_from_list(uint32_t id, uint32_t *list, uint8_t *count);
void get_command(const struct can2040_msg *received_msg);

#endif