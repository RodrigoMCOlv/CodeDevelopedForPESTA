/**
 * * AUTHOR: Rodrigo Oliveira
 * DATE: 30/05/2025
 * CONTACT: rodrigo.c.oliveira@inesctec.pt
 * CONTACT: rodrigo.mc.oliveira@gmail.com
 * * DESCRIPTION: This file contains the implementation of the CAN bridge functions.
 * It, aditionally contain advanced filtering functions that are usable trough the CAN line
 */

 #include "CAN_bridge.h" 
 #include <string.h>    // For memcmp and memcpy
 #include <stdio.h>

#define RAND_MAX 1

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


void can_rx_callback(struct can2040 *can_instance_ptr, uint32_t id, uint8_t dlc, uint8_t *data_payload){
    struct can2040_msg received_msg;
    received_msg.id = id;
    received_msg.dlc = dlc;
    memcpy(received_msg.data, data_payload, dlc);

    uint8_t rx_interface_id;
    struct can2040 *target_can_instance_ptr;
    uint8_t tx_interface_id;

    //checkprint(&received_msg);

    if (can_instance_ptr == &cbus0) {
        rx_interface_id = CAN_IFACE0;
        target_can_instance_ptr = &cbus1;
        tx_interface_id = CAN_IFACE1;
    } else if (can_instance_ptr == &cbus1) {
        rx_interface_id = CAN_IFACE1;
        target_can_instance_ptr = &cbus0;
        tx_interface_id = CAN_IFACE0;
    } else {
        return;
    }

    if (is_echo(&received_msg, dlc, rx_interface_id)) {
        return;
    }

    if (id == CONTROL_ID) {
        get_command(&received_msg);
    } else {
        bool should_bridge = false;

        switch (current_filter_state) {
            case FILTER_MODE_PASSIVE:
                should_bridge = true;
                break;

            case FILTER_MODE_WHITELIST:
                if (find_id_in_list(id, whitelist_ids, whitelist_count) != -1) {
                    should_bridge = true;
                }
                break;
            case FILTER_MODE_BLACKLIST:
                if (find_id_in_list(id, blacklist_ids, blacklist_count) == -1) {
                    should_bridge = true;
                }
                break;
            case FILTER_MODE_ZERO_TO_ONE: // Only CAN0 -> CAN1
                if (rx_interface_id == CAN_IFACE0) {
                    should_bridge = true;
                }
                break;
            case FILTER_MODE_ONE_TO_ZERO: // Only CAN1 -> CAN0
                if (rx_interface_id == CAN_IFACE1) {
                    should_bridge = true;
                }
                break;
            case FILTER_MODE_ONE_WAY_1_TO_0_BLACKLIST:
                if (rx_interface_id == CAN_IFACE1) { // Message from CAN1
                    if (find_id_in_list(id, blacklist_ids, blacklist_count) == -1) { // Not in blacklist
                        should_bridge = true;
                    }
                }
                break;
            case FILTER_MODE_ONE_WAY_1_TO_0_WHITELIST:
                if (rx_interface_id == CAN_IFACE1) { // Message from CAN1
                    if (find_id_in_list(id, whitelist_ids, whitelist_count) != -1) { // In whitelist
                        should_bridge = true;
                    }
                }
                break;
            case FILTER_MODE_ONE_WAY_0_TO_1_BLACKLIST:
                if (rx_interface_id == CAN_IFACE0) { // Message from CAN0
                    if (find_id_in_list(id, blacklist_ids, blacklist_count) == -1) { // Not in blacklist
                        should_bridge = true;
                    }
                }
                break;
            case FILTER_MODE_ONE_WAY_0_TO_1_WHITELIST:
                if (rx_interface_id == CAN_IFACE0) { // Message from CAN0
                    if (find_id_in_list(id, whitelist_ids, whitelist_count) != -1) { // In whitelist
                        should_bridge = true;
                    }
                }
                break;
            case FILTER_MODE_ONE_WAY_1_TO_0_EXCEPT:
                if (rx_interface_id == CAN_IFACE1) { // Message from CAN1
                    should_bridge = true;
                } else if (rx_interface_id == CAN_IFACE0) { // Message from CAN0
                    if (find_id_in_list(id, exception_ids, exception_count) != -1) { // In whitelist
                        should_bridge = true;
                    }
                }
                break;
            case FILTER_MODE_ONE_WAY_0_TO_1_EXCEPT:
                if (rx_interface_id == CAN_IFACE0) { // Message from CAN0
                    should_bridge = true;
                } else if (rx_interface_id == CAN_IFACE1) { // Message from CAN1
                    if (find_id_in_list(id, exception_ids, exception_count) != -1) { // In whitelist
                        should_bridge = true;
                    }
                }
                break;
            case FILTER_MODE_BIDIRECTIONAL_EXCEPT_OW_0_TO_1:
                if (find_id_in_list(id, one_way_restricted_ids, one_way_restricted_count) != -1) {
                    if (rx_interface_id == CAN_IFACE0) {
                        should_bridge = true; // Allow 0->1
                    } else { // rx_interface_id == CAN_IFACE1
                        should_bridge = false; // Block 1->0 for this ID
                    }
                } else {
                    should_bridge = true; // Not restricted, allow bidirectional
                }
                break;
            case FILTER_MODE_BIDIRECTIONAL_EXCEPT_OW_1_TO_0:
                if (find_id_in_list(id, one_way_restricted_ids, one_way_restricted_count) != -1) {
                    if (rx_interface_id == CAN_IFACE1) {
                        should_bridge = true; // Allow 1->0
                    } else { // rx_interface_id == CAN_IFACE0
                        should_bridge = false; // Block 0->1 for this ID
                    }
                } else {
                    should_bridge = true; // Not restricted, allow bidirectional
                }
                break;
            default:
                should_bridge = false;
                break;
        }
        if (should_bridge) {
            bridge_transmit(target_can_instance_ptr, &received_msg, dlc, tx_interface_id);
        }
    }
 }

 static uint32_t get_id_from_data(const uint8_t *data) {
     return (uint32_t)((data[3] << 24) | (data[4] << 16) | (data[5] << 8) | data[6]);
 }

 static int8_t find_id_in_list(uint32_t id, const uint32_t *list, uint8_t count) {
     for (uint8_t i = 0; i < count; ++i) {
         if (list[i] == id) {
             return i;
         }
     }
     return -1;
 }

 static bool add_id_to_list(uint32_t id, uint32_t *list, uint8_t *count, uint8_t max_size) {
     if (*count >= max_size) {
         return false;
     }
     if (find_id_in_list(id, list, *count) != -1) {
         return false;
     }
     list[*count] = id;
     (*count)++;
     return true;
 }

 static bool remove_id_from_list(uint32_t id, uint32_t *list, uint8_t *count) {
     int8_t index = find_id_in_list(id, list, *count);
     if (index != -1) {
         for (uint8_t i = (uint8_t)index; i < (*count - 1); ++i) {
             list[i] = list[i+1];
         }
         (*count)--;
         return true;
     }
     return false;
 }

 void get_command(const struct can2040_msg *received_msg) {

    
    if(received_msg->data[7] != (received_msg->data[0]+received_msg->data[1]+received_msg->data[2]+received_msg->data[3]+received_msg->data[4]+received_msg->data[5]+received_msg->data[6] % 255)){

        struct can2040_msg error_msg;
        error_msg.id = FEEDBACK_ID;
        error_msg.data[0] = received_msg->data[0];
        error_msg.data[1] = ERROR_IN_MESSAGE;
        error_msg.data[7] = (error_msg.data[0] + error_msg.data[1]) % 256;
   
        struct can2040 *feedback_bus;

        bridge_transmit( &CAN_COMPUTER_BUS, &error_msg, error_msg.dlc, CAN_COMPUTER_IFACE);

        return;
    }
    

    uint8_t command_mode = received_msg->data[0];
    uint8_t action = received_msg->data[1];
    uint32_t id_to_process;

    if (!bridge_enabled && command_mode != TURNON) {
        return;
    }

    uint32_t *list_to_use = NULL;
    uint8_t *count_to_use = NULL;
    FilterMode_t target_filter_mode = current_filter_state;
    bool is_list_based_command = false;

    switch (command_mode) {
        case WHITELIST_MODE:
            list_to_use = whitelist_ids;
            count_to_use = &whitelist_count;
            target_filter_mode = FILTER_MODE_WHITELIST;
            is_list_based_command = true;
            break;

        case BLACKLIST_MODE:
            list_to_use = blacklist_ids;
            count_to_use = &blacklist_count;
            target_filter_mode = FILTER_MODE_BLACKLIST;
            is_list_based_command = true;
            break;

        case OW_1_TO_0_BLACKLIST_MODE:
            list_to_use = blacklist_ids;
            count_to_use = &blacklist_count;
            target_filter_mode = FILTER_MODE_ONE_WAY_1_TO_0_BLACKLIST;
            is_list_based_command = true;
            break;

        case OW_1_TO_0_WHITELIST_MODE:
            list_to_use = whitelist_ids;
            count_to_use = &whitelist_count;
            target_filter_mode = FILTER_MODE_ONE_WAY_1_TO_0_WHITELIST;
            is_list_based_command = true;
            break;

        case OW_0_TO_1_BLACKLIST_MODE:
            list_to_use = blacklist_ids;
            count_to_use = &blacklist_count;
            target_filter_mode = FILTER_MODE_ONE_WAY_0_TO_1_BLACKLIST;
            is_list_based_command = true;
            break;

        case OW_0_TO_1_WHITELIST_MODE:
            list_to_use = whitelist_ids;
            count_to_use = &whitelist_count;
            target_filter_mode = FILTER_MODE_ONE_WAY_0_TO_1_WHITELIST;
            is_list_based_command = true;
            break;

        case OW_1_TO_0_EXCEPT_MODE:
            list_to_use = exception_ids;
            count_to_use = &exception_count;
            target_filter_mode = FILTER_MODE_ONE_WAY_1_TO_0_EXCEPT;
            is_list_based_command = true;
            break;

        case OW_0_TO_1_EXCEPT_MODE:
            list_to_use = exception_ids;
            count_to_use = &exception_count;
            target_filter_mode = FILTER_MODE_ONE_WAY_0_TO_1_EXCEPT;
            is_list_based_command = true;
            break;

        case BI_EXCEPT_OW_0_TO_1_MODE:
            list_to_use = one_way_restricted_ids;
            count_to_use = &one_way_restricted_count;
            target_filter_mode = FILTER_MODE_BIDIRECTIONAL_EXCEPT_OW_0_TO_1;
            is_list_based_command = true;
            break;

        case BI_EXCEPT_OW_1_TO_0_MODE:
            list_to_use = one_way_restricted_ids;
            count_to_use = &one_way_restricted_count;
            target_filter_mode = FILTER_MODE_BIDIRECTIONAL_EXCEPT_OW_1_TO_0;
            is_list_based_command = true;
            break;

        case PASSIVE_MODE:
            current_filter_state = FILTER_MODE_PASSIVE;
            break;

        case ONE_WAY_ZERO_TO_ONE_MODE:
            current_filter_state = FILTER_MODE_ZERO_TO_ONE;
            break;
            
        case ONE_WAY_ONE_TO_ZERO_MODE:
            current_filter_state = FILTER_MODE_ONE_TO_ZERO;
            break;
        case TURNOFF:
             if (action == CONFIRM) {
                 bridge_enabled = false;
             }
             break;
         case TURNON:
             if (action == CONFIRM) {
                 bridge_enabled = true;
             }
             break;
         default:
             return;
     }

     if (is_list_based_command && list_to_use != NULL && count_to_use != NULL) {
         switch (action) {
             case SET_MODE:
                 current_filter_state = target_filter_mode;
                 break;
             case ADD_ID:
                 id_to_process = get_id_from_data(received_msg->data);
                 add_id_to_list(id_to_process, list_to_use, count_to_use, MAX_LIST_SIZE);
                 break;
             case REMOVE_ID:
                 id_to_process = get_id_from_data(received_msg->data);
                 remove_id_from_list(id_to_process, list_to_use, count_to_use);
                 break;
             case CLEAR_LIST:
                 *count_to_use = 0;
                 break;
             case SET_MODE_AND_CLEAR:
                 current_filter_state = target_filter_mode;
                 *count_to_use = 0;
                 break;
             case SET_MODE_ADD_ID:
                 current_filter_state = target_filter_mode;
                 id_to_process = get_id_from_data(received_msg->data);
                 add_id_to_list(id_to_process, list_to_use, count_to_use, MAX_LIST_SIZE);
                 break;
             default:
                 break;
         }
     }
 }
