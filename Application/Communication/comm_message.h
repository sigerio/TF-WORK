#ifndef COMM_MESSAGE_H
#define COMM_MESSAGE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef enum {
    DATA_TO_MAIN = 0,
    DATA_TO_MACHINE,
    DATA_TO_POLE,
    DATA_TO_SOLAR,
    DATA_TO_MT80,
    DATA_FROM_MAIN,
    DATA_FROM_MACHINE,
    DATA_FROM_POLE,
    DATA_FROM_SOLAR,
    DATA_FROM_MT80,
    DATA_MARK_CNT,
    DATA_MARK_NONE = -1,
} DATA_MARK_E;

#define MY_MESSAGE_DATA_SIZE 256U

typedef struct my_message_node {
    uint8_t *data;
    uint16_t data_len;
    DATA_MARK_E data_mark;
} my_message_node_t;

#endif
