#ifndef COMM_MESSAGE_H
#define COMM_MESSAGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef enum
{
    
    DATA_TO_MAIN = 0,
    DATA_TO_WIND,   //主站询问 2
    DATA_TO_RAIN,   //主站询问 3
    DATA_TO_WATER,  //主站询问 4
    DATA_TO_TC,     //主站询问 5
    DATA_TO_4G,



    DATA_FROM_MAIN,
    DATA_FROM_WIND,  
    DATA_FROM_RAIN,  
    DATA_FROM_WATER,
    DATA_FROM_TC,   
    DATA_FROM_4G,


    
    DATA_MARK_CNT,
    DATA_MARK_NONE = -1,
}DATA_MARK_E;

#define MY_MESSAGE_DATA_SIZE 256U

typedef struct my_message_node {
    uint8_t *data;
    uint16_t data_len;
    DATA_MARK_E data_mark;
} my_message_node_t;

#endif
