#include "esp8266.h"
#include "usart.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "oled.h"

/*
*************************************
宏定义
*************************************
*/
#define WIFI_SSID        "stm32"
#define WIFI_PASSWD      "123456789"

#define MQTT_CLIENT_ID   "k207aIgk2i4.stm32|securemode=2\\,signmethod=hmacsha256\\,timestamp=1733147538233|"   
#define MQTT_USER_NAME   "stm32&k207aIgk2i4"
#define MQTT_PASSWD      "62b9041daba64550495738f21af3437d479e15e1ccc2630623d45bfec0793e97"
#define BROKER_ASDDRESS  "iot-06z00gysrslmiv9.mqtt.iothub.aliyuncs.com"
#define SUB_TOPIC        "/sys/k207aIgk2i4/stm32/thing/service/property/set"
#define PUB_TOPIC        "/sys/k207aIgk2i4/stm32/thing/event/property/post"
#define JSON_FORMAT      "{\\\"params\\\":{\\\"ctrl_Motor\\\":%d\\,\\\"ctrl_Alarm\\\":%d\\}\\,\\\"version\\\":\\\"1.0.0\\\"}"

unsigned char receive_buf[512];	  //串口2接收缓存数组
unsigned char receive_start = 0;	//串口2接收开始标志位
uint16_t receive_count = 0;	      //串口2接收数据计数器
uint16_t receive_finish = 0;	    //串口2接收结束标志位 

/**
  * @brief          串口1数据接收处理函数
  * @param[in]      none
  * @retval         none
  */
void uart1_receiver_handle(void)
{
	unsigned char receive_data = 0;
	unsigned char res;
	if (USART_GetITStatus(USART1, USART_IT_RXNE) != RESET)  //接收中断
	{
		receive_data = USART_ReceiveData(USART1);//(USART1->DR);	//读取接收到的数据
		USART_SendData(USART2, receive_data);
		receive_buf[receive_count++] = receive_data;
		receive_start = 1;	                              //串口1接收数据开始标志位置1
		receive_finish = 0;	                              //串口1接收数据完成标志位清0
	}
}
/**
  * @brief          串口1数据接收清0函数
  * @param[in]      len:清空的数据长度
  * @retval         none
  */
void uart1_receiver_clear(uint16_t len)
{
	memset(receive_buf, 0x00, len);
	receive_count = 0;
	receive_start = 0;
	receive_finish = 0;
}
/**
  * @brief          esp8266发送命令函数
  * @param[in]      cmd:发送的命令,len:命令的长度,rec_data:期望接收数据
  * @retval         none
  */
uint8_t esp8266_send_cmd(uint8_t* cmd, unsigned char len, char* rec_data)
{
	unsigned char retval = 0;
	unsigned int count = 0;
	uint8_t* p = cmd;
	u8 i;

	for (i = 0; i < len; i++) {
		while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);
		USART_SendData(USART1, p[i]);
	}

	while ((receive_start == 0) && (count < 1000))//最多等待1000ms
	{
		count++;
		delay_ms(1);
	}

	if (count >= 1000)
	{
		retval = 1;
	}
	else
	{
		do
		{
			receive_finish++;
			delay_ms(1);
		} while (receive_finish < 500);
		retval = 2;
		if (strstr((const char*)receive_buf, rec_data))	//判断发会的数据是否为OK
		{
			retval = 0;
		}
	}
	uart1_receiver_clear(receive_count);
	return retval;
}

/**
  * @brief          esp8266复位
  * @param[in]      none
  * @retval         返回0复位成功,返回1复位失败
  */
uint8_t esp8266_reset(void)
{
	uint8_t retval = 0;
	uint16_t count = 0;
	u8 len = strlen("AT+RST\r\n");
	uint8_t* p = (uint8_t*)"AT+RST\r\n";
	u8 i;
	for (i = 0; i < len; i++) {
		while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);
		USART_SendData(USART1, p[i]);
	}
	while ((receive_start == 0) && (count < 2000))
	{
		count++;
		delay_ms(1);
	}
	if (count >= 2000)
	{
		retval = 1;
	}
	else
	{
		delay_ms(5000);
		if (strstr((const char*)receive_buf, "OK"))
		{
			retval = 0;
		}
		else
		{
			retval = 1;
		}
	}
	uart1_receiver_clear(receive_count);
	return retval;
}
/**
  * @brief          esp8266发送数据
  * @param[in]      none
  * @retval         返回0发送数据成功,返回1发送数据失败
  */
uint8_t esp8266_send_msg(void)
{
	uint8_t retval = 0;
	uint16_t count = 0;
	u8 i;
	static uint8_t error_count = 0;
	unsigned char msg_buf[256];
	sprintf((char*)msg_buf, "AT+MQTTPUB=0,\""PUB_TOPIC"\",\""JSON_FORMAT"\",0,0\r\n", ctrl_Motor, ctrl_Alarm);
	uint8_t* p = (uint8_t*)msg_buf;
	for (i = 0; i < strlen((const char*)msg_buf); i++) {
		while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET);
		USART_SendData(USART1, p[i]);
	}
	while ((receive_start == 0) && (count < 500))
	{
		count++;
		delay_ms(1);
	}
	if (count >= 500)
	{
		retval = 1;
	}
	else
	{
		delay_ms(50);
		if (strstr((const char*)receive_buf, "OK"))
		{
			retval = 0;
			error_count = 0;
		}
		else
		{
			error_count++;
			if (error_count == 5)
			{
				error_count = 0;
				esp8266_init();
			}
		}
	}
	uart1_receiver_clear(receive_count);
	return retval;
}
/**
  * @brief          esp8266接收数据
  * @param[in]      none
  * @retval         返回0接收数据正常,返回1接收数据异常或无数据
  */
uint8_t esp8266_receive_msg(void)
{
	u8 i;
	uint8_t retval = 0;
	int msg_len = 0;
	uint8_t msg_body[128] = { 0 };

	if (receive_start == 1)
	{
		do
		{
			receive_finish++;
			delay_ms(1);
		} while (receive_finish < 500);
		if (strstr((const char*)receive_buf, "+MQTTSUBRECV:"))
		{
			sscanf((const char*)receive_buf, "+MQTTSUBRECV:0,\""SUB_TOPIC"\",%d,%s", &msg_len, msg_body);
			if (strlen((const char*)msg_body) == msg_len)
			{
				const char* key[] = { "\"ctrl_Motor\":","\"ctrl_Alarm\":","\"day\":","\"weekday\":","\"hour\":","\"min\":","\"sec\":" };
				i = 0;
				const char* start = strstr((const char*)receive_buf, key[i]);				// 
				while (start == NULL) {
					i++;
					start = strstr((const char*)receive_buf, key[i]);
					if (start != NULL)break;
					if (i == 6) {
						return 1;
					}
				}
				// 将 start 移动到数字开始的位置（跳过 key 和冒号）
				start += strlen(key[i]);

				// 跳过任何可能的空格
				while (*start == ' ' || *start == '\t') {
					start++;
				}

				// 解析数字部分
				int value = 0;
				while (*start >= '0' && *start <= '9') {
					value = value * 10 + (*start - '0');
					start++;
				}
				switch (i) {
				case 0:
				{
					ctrl_Motor = value;
					break;
				}

				case 1:
				{
					ctrl_Alarm = value;
					break;
				}


				case 2:
				{
					day = value;
					break;
				}

				case 3:
				{
					weekday = value;
					break;
				}

				case 4:
				{
					hour = value;
					break;
				}

				case 5:
				{
					min = value;
					break;
				}

				case 6:
				{
					sec = value;
					break;
				}
				}
			}
			else
			{
				retval = 1;
			}
		}
		else
		{
			retval = 1;
		}
	}
	else
	{
		retval = 1;
	}
	uart1_receiver_clear(receive_count);
	return retval;
}
/**
  * @brief          esp8266初始化
  * @param[in]      none
  * @retval         none
  */
void esp8266_init(void)
{
	uart_init(115200);           											//初始化串口1
	OLED_ShowString(6, 10, "1.SETTING STATION MODE", 12);
	OLED_Refresh();
	delay_ms(2000);
	OLED_Clear();
	while (esp8266_send_cmd((uint8_t*)"AT+CWMODE=1\r\n", strlen("AT+CWMODE=1\r\n"), "OK") != 0)
	{
		delay_ms(1000);
	}
	OLED_ShowString(6, 10, "2.CLOSE ESP8266 ECHO", 12);
	OLED_Refresh();
	delay_ms(2000);
	OLED_Clear();
	while (esp8266_send_cmd((uint8_t*)"ATE0\r\n", strlen("ATE0\r\n"), "OK") != 0)
	{
		delay_ms(1000);
	}
	OLED_ShowString(6, 10, "3.NO AUTO CONNECT WIFI", 12);
	OLED_Refresh();
	delay_ms(2000);
	OLED_Clear();
	while (esp8266_send_cmd((uint8_t*)"AT+CWAUTOCONN=0\r\n", strlen("AT+CWAUTOCONN=0\r\n"), "OK") != 0)
	{
		delay_ms(1000);
	}
	OLED_ShowString(6, 10, "4.RESET ESP8266", 12);
	OLED_Refresh();
	delay_ms(2000);
	OLED_Clear();
	while (esp8266_reset() != 0)
	{
		delay_ms(5000);
	}
	OLED_ShowString(6, 10, "5.CONFIG WIFI NETWORK", 12);
	OLED_Refresh();
	delay_ms(2000);
	OLED_Clear();
	while (esp8266_send_cmd((uint8_t*)"AT+CWJAP=\""WIFI_SSID"\",\""WIFI_PASSWD"\"\r\n",
		strlen("AT+CWJAP=\""WIFI_SSID"\",\""WIFI_PASSWD"\"\r\n"), "OK") != 0)
	{
		delay_ms(8000);
	}
	OLED_ShowString(6, 10, "6.MQTT USER CONFIG", 12);
	OLED_Refresh();
	delay_ms(2000);
	OLED_Clear();

	while (esp8266_send_cmd((uint8_t*)"AT+MQTTUSERCFG=0,1,\""MQTT_CLIENT_ID"\",\""MQTT_USER_NAME"\",\""MQTT_PASSWD"\",0,0,\"\"\r\n",
		strlen("AT+MQTTUSERCFG=0,1,\""MQTT_CLIENT_ID"\",\""MQTT_USER_NAME"\",\""MQTT_PASSWD"\",0,0,\"\"\r\n"), "OK") != 0)
	{
		delay_ms(2000);
	}
	OLED_ShowString(6, 10, "7.MQTT BROKER", 12);
	OLED_Refresh();
	delay_ms(2000);
	OLED_Clear();
	while (esp8266_send_cmd((uint8_t*)"AT+MQTTCONN=0,\""BROKER_ASDDRESS"\",1883,0\r\n",
		strlen("AT+MQTTCONN=0,\""BROKER_ASDDRESS"\",1883,0\r\n"), "OK") != 0)
	{
		delay_ms(8000);
	}
	OLED_ShowString(6, 10, "8.SUBSCRIBE TOPIC", 12);
	OLED_Refresh();
	delay_ms(2000);
	OLED_Clear();
	while (esp8266_send_cmd((uint8_t*)"AT+MQTTSUB=0,\""SUB_TOPIC"\",0\r\n", strlen("AT+MQTTSUB=0,\""SUB_TOPIC"\",0\r\n"), "OK") != 0)
	{
		delay_ms(2000);
	}
	OLED_ShowString(6, 10, "9.ESP8266 INIT OK!!!", 12);
	OLED_Refresh();
	delay_ms(2000);
	OLED_Clear();
}
