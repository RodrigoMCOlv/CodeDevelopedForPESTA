/**
* AUTHOR: Rodrigo Oliveira
* DATE: 30/05/2025
*
* CONTACT: rodrigo.c.oliveira@inesctec.pt
* CONTACT: rodrigo.mc.oliveira@gmail.com
*
* * DESCRIPTION: This file contains the implementation of the CAN bridge functions.
* It aditionally contains advanced filtering functions that are usable trough the CAN line
**/

#include "CAN_bridge.h" 
#include <string.h>    // For memcmp and memcpy
#include <stdio.h>

// Separate circular buffers for messages transmitted *out* of each CAN interface
volatile struct transmitted_msg_info recent_tx_can0_msgs[RECENT_MESSAGES_BUFFER_SIZE];
volatile int recent_tx_can0_idx = 0;

volatile struct transmitted_msg_info recent_tx_can1_msgs[RECENT_MESSAGES_BUFFER_SIZE];
volatile int recent_tx_can1_idx = 0;
 

/**
* @brief Adds a message to the recent transmissions buffer for a specific interface.
*/
void add_recent_tx_message(const struct can2040_msg *msg, uint8_t dlc, uint8_t tx_interface_id) {
   if (tx_interface_id == CAN_IFACE0) {
        recent_tx_can0_msgs[recent_tx_can0_idx].id = msg->id;
        recent_tx_can0_msgs[recent_tx_can0_idx].dlc = dlc;
        memcpy((void *)recent_tx_can0_msgs[recent_tx_can0_idx].data, (const void *)msg->data, dlc);
        recent_tx_can0_idx = (recent_tx_can0_idx + 1) % RECENT_MESSAGES_BUFFER_SIZE;
    } else if (tx_interface_id == CAN_IFACE1) {
        recent_tx_can1_msgs[recent_tx_can1_idx].id = msg->id;
        recent_tx_can1_msgs[recent_tx_can1_idx].dlc = dlc;
        memcpy((void *)recent_tx_can1_msgs[recent_tx_can1_idx].data, (const void *)msg->data, dlc);
        recent_tx_can1_idx = (recent_tx_can1_idx + 1) % RECENT_MESSAGES_BUFFER_SIZE;
    }
}

 /**
  * @brief Checks if a received message is an echo of a message recently transmitted.
  */
bool is_echo(const struct can2040_msg *received_msg, uint8_t received_dlc, uint8_t rx_interface_id) {
    volatile struct transmitted_msg_info *buffer_to_check;
    int buffer_size = RECENT_MESSAGES_BUFFER_SIZE;

    if (rx_interface_id == CAN_IFACE0) {
        buffer_to_check = recent_tx_can0_msgs;
    } else if (rx_interface_id == CAN_IFACE1) {
        buffer_to_check = recent_tx_can1_msgs;
    } else {
        return false;
    }
    for (int i = 0; i < buffer_size; i++) {
        if (buffer_to_check[i].id == received_msg->id &&
            buffer_to_check[i].dlc == received_dlc) {
            if (memcmp((void *)buffer_to_check[i].data, (const void *)received_msg->data, received_dlc) == 0) {
                return true;
            }
        }
    }
    return false;
} 

/**
* @brief Passes a CAN message from one bus to another and records it.
*/
void bridge_transmit(struct can2040 *target_bus, struct can2040_msg *msg, uint8_t dlc, uint8_t tx_interface_id){

    msg->dlc = dlc;

    can_send(target_bus, msg);
    add_recent_tx_message(msg, dlc, tx_interface_id);
}



/**
* @brief Processes a CAN message received on a specific bus.
*/
void can_rx_callback(struct can2040 *can_instance_ptr, uint32_t id, uint8_t dlc, uint8_t *data_payload){
    struct can2040_msg received_msg;
    received_msg.id = id;
    received_msg.dlc = dlc;
    memcpy(received_msg.data, data_payload, dlc);

    uint8_t rx_interface_id;
    struct can2040 *target_can_instance_ptr;
    uint8_t tx_interface_id;

    //checkprint(&received_msg);

    if (can_instance_ptr == &cbus0) {           //If the message was received on the CAN bus 0 
        rx_interface_id = CAN_IFACE0;   
        target_can_instance_ptr = &cbus1;       //set bus 1 as the target
        tx_interface_id = CAN_IFACE1;
    } else if (can_instance_ptr == &cbus1) {    //If the message was received on the CAN bus 1
        rx_interface_id = CAN_IFACE1;
        target_can_instance_ptr = &cbus0;       //set bus 0 as the target
        tx_interface_id = CAN_IFACE0;
    } else {
        return;
    }

    if (is_echo(&received_msg, dlc, rx_interface_id)) { //If the message is an echo, ignore it
        return;
    }

    if (id == CONTROL_ID) {                             //If the message is a control message act on it
        get_command(&received_msg);
    } else {

        switch (current_filter_state) {
            case FILTER_MODE_PASSIVE:                   //If the filter mode is passive, bridge all messages
                should_bridge = true;
                break;

            case FILTER_MODE_WHITELIST:             
                if (find_id_in_list(id, whitelist_ids, whitelist_count) != -1) {
                    should_bridge = true;               //If the ID is in the whitelist, bridge the message
                }
                break;
            case FILTER_MODE_BLACKLIST:
                if (find_id_in_list(id, blacklist_ids, blacklist_count) == -1) {
                    should_bridge = true;               //If the ID is not in the blacklist, bridge the message
                }
                break;
            case FILTER_MODE_ZERO_TO_ONE: // Only CAN0 -> CAN1
                if (rx_interface_id == CAN_IFACE0) {
                    should_bridge = true;               //If the message was received on CAN0, bridge the message
                }
                break;
            case FILTER_MODE_ONE_TO_ZERO: // Only CAN1 -> CAN0
                if (rx_interface_id == CAN_IFACE1) {
                    should_bridge = true;               //If the message was received on CAN1, bridge the message
                }
                break;
            case FILTER_MODE_ONE_WAY_1_TO_0_BLACKLIST:
                if (rx_interface_id == CAN_IFACE1) {
                    if (find_id_in_list(id, blacklist_ids, blacklist_count) == -1) { 
                        should_bridge = true;           //If the ID is not in the blacklist and the message was received on CAN1, bridge the message
                    }
                }
                break;
            case FILTER_MODE_ONE_WAY_1_TO_0_WHITELIST:
                if (rx_interface_id == CAN_IFACE1) {
                    if (find_id_in_list(id, whitelist_ids, whitelist_count) != -1) { 
                        should_bridge = true;           //If the ID is in the whitelist and the message was received on CAN1, bridge the message
                    }
                }
                break;
            case FILTER_MODE_ONE_WAY_0_TO_1_BLACKLIST:
                if (rx_interface_id == CAN_IFACE0) { 
                    if (find_id_in_list(id, blacklist_ids, blacklist_count) == -1) { 
                        should_bridge = true;           //If the ID is not in the blacklist and the message was received on CAN0, bridge the message
                    }
                }
                break;
            case FILTER_MODE_ONE_WAY_0_TO_1_WHITELIST:
                if (rx_interface_id == CAN_IFACE0) { 
                    if (find_id_in_list(id, whitelist_ids, whitelist_count) != -1) { 
                        should_bridge = true;           //If the ID is in the whitelist and the message was received on CAN0, bridge the message
                    }
                }
                break;
            case FILTER_MODE_ONE_WAY_1_TO_0_EXCEPT:
                if (rx_interface_id == CAN_IFACE1) { 
                    should_bridge = true;
                } else if (rx_interface_id == CAN_IFACE0) { 
                    if (find_id_in_list(id, exception_ids, exception_count) != -1) { 
                        should_bridge = true;           //If the ID is in the exception list and the message was received on CAN0, bridge the message
                                                        //If the message was received on CAN1, bridge the message
                    }
                }
                break;
            case FILTER_MODE_ONE_WAY_0_TO_1_EXCEPT:
                if (rx_interface_id == CAN_IFACE0) {
                    should_bridge = true;
                } else if (rx_interface_id == CAN_IFACE1) { 
                    if (find_id_in_list(id, exception_ids, exception_count) != -1) {
                        should_bridge = true;           //If the ID is in the exception list and the message was received on CAN1, bridge the message
                                                        //If the message was received on CAN0, bridge the message
                    }
                }
                break;
            case FILTER_MODE_BIDIRECTIONAL_EXCEPT_OW_0_TO_1:
                if (find_id_in_list(id, one_way_restricted_ids, one_way_restricted_count) != -1) {
                    if (rx_interface_id == CAN_IFACE0) {
                        should_bridge = true;           //If the message was received on CAN0, bridge the message
                    } else { 
                        should_bridge = false;          //Block 1->0 for this ID
                    }
                } else {
                    should_bridge = true;               //Not restricted, allow bidirectional
                }
                break;
            case FILTER_MODE_BIDIRECTIONAL_EXCEPT_OW_1_TO_0:
                if (find_id_in_list(id, one_way_restricted_ids, one_way_restricted_count) != -1) {
                    if (rx_interface_id == CAN_IFACE1) {
                        should_bridge = true;           //If the message was received on CAN1, bridge the message
                    } else { /
                        should_bridge = false;          //Block 0->1 for this ID
                    }
                } else {
                    should_bridge = true;               //Not restricted, allow bidirectional
                }
                break;

            default:
                should_bridge = false;                  //Default, don't bridge
                break;
        }
        if (should_bridge) {
            bridge_transmit(target_can_instance_ptr, &received_msg, dlc, tx_interface_id);  //Bridge the message
        }
    }
}

/**
* @brief Gets the ID from the CAN message data.
*/
static uint32_t get_id_from_data(const uint8_t *data) {
    return (uint32_t)((data[3] << 24) | (data[4] << 16) | (data[5] << 8) | data[6]);
}


/**
* @brief Finds the index of an ID in a list.
*/
static int8_t find_id_in_list(uint32_t id, const uint32_t *list, uint8_t count) {
    for (uint8_t i = 0; i < count; ++i) {
        if (list[i] == id) {
            return i;       //If the ID is found, return the index
        }
    }
    return -1;              //If the ID is not found, return -1
}


/**
* @brief Adds an ID to a list.
*/
static bool add_id_to_list(uint32_t id, uint32_t *list, uint8_t *count, uint8_t max_size) {
    if (*count >= max_size) {
        return false;       //If the list is full, return false
    }
    if (find_id_in_list(id, list, *count) != -1) {
        return false;       //If the ID is already in the list, return false
    }
    list[*count] = id;      
    (*count)++;
    return true;            //If the ID is added successfully, return true
}

/**
* @brief Removes an ID from a list.
*/
static bool remove_id_from_list(uint32_t id, uint32_t *list, uint8_t *count) {
    int8_t index = find_id_in_list(id, list, *count);
    if (index != -1) {
        for (uint8_t i = (uint8_t)index; i < (*count - 1); ++i) {   
            //Shift all IDs after the removed ID to the left
            list[i] = list[i+1];
        }
        (*count)--;
        return true;        //If the ID is removed successfully, return true
    }
    return false;           //If the ID is not in the list, return false
}

/** 
*   @brief Gets the command from the CAN message data. 
*/
void get_command(const struct can2040_msg *received_msg) {
    
    if(received_msg->data[7] != (received_msg->data[0]+received_msg->data[1]+received_msg->data[2]+received_msg->data[3]+received_msg->data[4]+received_msg->data[5]+received_msg->data[6] % 255)){
        //Checksum calculation and error handling
        struct can2040_msg error_msg;
        error_msg.id = FEEDBACK_ID;
        error_msg.data[0] = received_msg->data[0];
        error_msg.data[1] = ERROR_IN_MESSAGE;
        error_msg.data[7] = (error_msg.data[0] + error_msg.data[1]) % 256;
   
        struct can2040 *feedback_bus;
        bridge_transmit( &CAN_COMPUTER_BUS, &error_msg, error_msg.dlc, CAN_COMPUTER_IFACE); 

        return;
    }
    

    uint8_t command_mode = received_msg->data[0];   //Check the command
    uint8_t action = received_msg->data[1];         //Check the action
    uint32_t id_to_process;                         //Check the ID if applicable

    if (!bridge_enabled && command_mode != TURNON) {    //If the bridge is disabled and the command is not to turn it on, return
        return;
    }

    uint32_t *list_to_use = NULL;
    uint8_t *count_to_use = NULL;
    FilterMode_t target_filter_mode = current_filter_state;
    bool is_list_based_command = false;

    switch (command_mode) {
        case WHITELIST_MODE:                                        //Whitelist mode: set mode as FILTER_MODE_WHITELIST and list as whitelist_ids
            list_to_use = whitelist_ids;
            count_to_use = &whitelist_count;
            target_filter_mode = FILTER_MODE_WHITELIST;
            is_list_based_command = true;
            break;

        case BLACKLIST_MODE:                                        //Blacklist mode: set mode as FILTER_MODE_BLACKLIST and list as blacklist_ids
            list_to_use = blacklist_ids;
            count_to_use = &blacklist_count;
            target_filter_mode = FILTER_MODE_BLACKLIST;
            is_list_based_command = true;
            break;

        case OW_1_TO_0_WHITELIST_MODE:                              //One way 1 to 0 whitelist mode: set mode as FILTER_MODE_ONE_WAY_1_TO_0_WHITELIST and list as whitelist_ids
            list_to_use = whitelist_ids;
            count_to_use = &whitelist_count;
            target_filter_mode = FILTER_MODE_ONE_WAY_1_TO_0_WHITELIST;
            is_list_based_command = true;
            break;

        case OW_1_TO_0_BLACKLIST_MODE:                              //One way 1 to 0 blacklist mode: set mode as FILTER_MODE_ONE_WAY_1_TO_0_BLACKLIST and list as blacklist_ids
            list_to_use = blacklist_ids;
            count_to_use = &blacklist_count;
            target_filter_mode = FILTER_MODE_ONE_WAY_1_TO_0_BLACKLIST;
            is_list_based_command = true;
            break;

        case OW_0_TO_1_WHITELIST_MODE:                              //One way 0 to 1 whitelist mode: set mode as FILTER_MODE_ONE_WAY_0_TO_1_WHITELIST and list as whitelist_ids
            list_to_use = whitelist_ids;
            count_to_use = &whitelist_count;
            target_filter_mode = FILTER_MODE_ONE_WAY_0_TO_1_WHITELIST;
            is_list_based_command = true;
            break;
        
        case OW_0_TO_1_BLACKLIST_MODE:                              //One way 0 to 1 blacklist mode: set mode as FILTER_MODE_ONE_WAY_0_TO_1_BLACKLIST and list as blacklist_ids
            list_to_use = blacklist_ids;
            count_to_use = &blacklist_count;
            target_filter_mode = FILTER_MODE_ONE_WAY_0_TO_1_BLACKLIST;
            is_list_based_command = true;
            break;

        case OW_1_TO_0_EXCEPT_MODE:                                 //One way 1 to 0 except mode: set mode as FILTER_MODE_ONE_WAY_1_TO_0_EXCEPT and list as exception_ids
            list_to_use = exception_ids;
            count_to_use = &exception_count;
            target_filter_mode = FILTER_MODE_ONE_WAY_1_TO_0_EXCEPT;
            is_list_based_command = true;
            break;

        case OW_0_TO_1_EXCEPT_MODE:                                 //One way 0 to 1 except mode: set mode as FILTER_MODE_ONE_WAY_0_TO_1_EXCEPT and list as exception_ids
            list_to_use = exception_ids;
            count_to_use = &exception_count;
            target_filter_mode = FILTER_MODE_ONE_WAY_0_TO_1_EXCEPT;
            is_list_based_command = true;
            break;

        case BI_EXCEPT_OW_0_TO_1_MODE:                              //Bidirectional except one way 0 to 1 mode: set mode as FILTER_MODE_BIDIRECTIONAL_EXCEPT_OW_0_TO_1 and list as one_way_restricted_ids
            list_to_use = one_way_restricted_ids;
            count_to_use = &one_way_restricted_count;
            target_filter_mode = FILTER_MODE_BIDIRECTIONAL_EXCEPT_OW_0_TO_1;
            is_list_based_command = true;
            break;

        case BI_EXCEPT_OW_1_TO_0_MODE:                              //Bidirectional except one way 1 to 0 mode: set mode as FILTER_MODE_BIDIRECTIONAL_EXCEPT_OW_1_TO_0 and list as one_way_restricted_ids
            list_to_use = one_way_restricted_ids;
            count_to_use = &one_way_restricted_count;
            target_filter_mode = FILTER_MODE_BIDIRECTIONAL_EXCEPT_OW_1_TO_0;
            is_list_based_command = true;
            break;

        case ONE_WAY_ZERO_TO_ONE_MODE:                              //One way 0 to 1 mode: set mode as FILTER_MODE_ZERO_TO_ONE
            current_filter_state = FILTER_MODE_ZERO_TO_ONE;
            break;
            
        case ONE_WAY_ONE_TO_ZERO_MODE:                              //One way 1 to 0 mode: set mode as FILTER_MODE_ONE_TO_ZERO
            current_filter_state = FILTER_MODE_ONE_TO_ZERO;
            break;

        case TURNOFF:                                               //Turn off the bridge if action is CONFIRM
            if (action == CONFIRM) {
                bridge_enabled = false;
            }
            break;
        
        case TURNON:                                                //Turn on the bridge if action is CONFIRM
            if (action == CONFIRM) {
                bridge_enabled = true;
            }
            break;
        default:
            return;

        case PASSIVE_MODE:                                          //Passive mode: set mode as FILTER_MODE_PASSIVE bridge everything
            current_filter_state = FILTER_MODE_PASSIVE;
            break;

     }

     if (is_list_based_command && list_to_use != NULL && count_to_use != NULL) {         //If the command is a list based command
         switch (action) {
             case SET_MODE:                                                              //If the action is SET_MODE then set the mode
                 current_filter_state = target_filter_mode;
                 break;
             case ADD_ID:                                                                //If the action is ADD_ID then add the ID to the list
                 id_to_process = get_id_from_data(received_msg->data);
                 add_id_to_list(id_to_process, list_to_use, count_to_use, MAX_LIST_SIZE);
                 break;
             case REMOVE_ID:                                                             //If the action is REMOVE_ID then remove the ID from the list
                 id_to_process = get_id_from_data(received_msg->data);
                 remove_id_from_list(id_to_process, list_to_use, count_to_use);
                 break;
             case CLEAR_LIST:                                                            //If the action is CLEAR_LIST then clear the list
                 *count_to_use = 0;
                 break;
             case SET_MODE_AND_CLEAR:                                                    //If the action is SET_MODE_AND_CLEAR then set the mode and clear the list
                 current_filter_state = target_filter_mode;
                 *count_to_use = 0;
                 break;
             case SET_MODE_ADD_ID:                                                       //If the action is SET_MODE_ADD_ID then set the mode and add the ID to the list
                 current_filter_state = target_filter_mode;
                 id_to_process = get_id_from_data(received_msg->data);
                 add_id_to_list(id_to_process, list_to_use, count_to_use, MAX_LIST_SIZE);
                 break;
             default:
                 break;
         }
     }
 }
