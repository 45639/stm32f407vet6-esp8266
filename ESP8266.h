#ifndef _ESP8266_H_
#define _ESP8266_H_

#include "stm32f4xx.h"
extern uint8_t ctrl_Motor, ctrl_Alarm, day, weekday, hour, min, sec;

void uart2_receiver_handle(void);
uint8_t esp8266_receive_msg(void);
uint8_t esp8266_send_msg(void);
void esp8266_init(void);
#endif
