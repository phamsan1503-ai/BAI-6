#include "stm32f10x.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_i2c.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_usart.h"
#include "stdio.h"


#define BH1750_ADDR   (0x23<<1)

// ------------ Prototype -------------
void UART1_Config(void);
void I2C2_Config(void);
void GPIO_Config_I2C2(void);
void BH1750_Config(void);
uint16_t BH1750_GetLux(void);
void I2C2_Write(uint8_t addr, uint8_t data);
void I2C2_Read(uint8_t addr, uint8_t *data, uint8_t size);
void delay_ms(uint32_t ms);

// redirect printf
struct __FILE { int handle; };
FILE __stdout;
int fputc(int ch, FILE *f);



// ------------ BH1750 -------------

void BH1750_Config(void){
    uint8_t lenh [] = {0x01, 0x07, 0x10}; // Power ON, Reset, Continuous H-Resolution Mode
    for (uint8_t i = 0; i < sizeof(lenh); i++) {
        I2C2_Write(BH1750_ADDR, lenh[i]);
    }
}


uint16_t BH1750_GetLux(void){
    uint8_t raw_data[2];
    uint16_t raw_value;

    I2C2_Read(BH1750_ADDR, raw_data, 2);
    raw_value = (raw_data[0] << 8) | raw_data[1];

    // Tuong duong raw_value / 1.2, s? d?ng s? nguyên
    return (raw_value * 5) / 6; // 1/1.2 ˜ 5/6
}


void GPIO_Config_I2C2(void){
    GPIO_InitTypeDef gpio;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    gpio.GPIO_Pin   = GPIO_Pin_10 | GPIO_Pin_11;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode  = GPIO_Mode_AF_OD;
    GPIO_Init(GPIOB, &gpio);
}

void I2C2_Config(void){
    I2C_InitTypeDef i2c;
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C2, ENABLE);

    I2C_DeInit(I2C2);

    i2c.I2C_ClockSpeed = 100000;
    i2c.I2C_Mode = I2C_Mode_I2C;
    i2c.I2C_DutyCycle = I2C_DutyCycle_2;
    i2c.I2C_OwnAddress1 = 0x00;
    i2c.I2C_Ack = I2C_Ack_Enable;
    i2c.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;

    I2C_Init(I2C2, &i2c);
    I2C_Cmd(I2C2, ENABLE);
}

void I2C2_Write(uint8_t addr, uint8_t data){
    I2C_GenerateSTART(I2C2, ENABLE);
    while(!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_MODE_SELECT));

    I2C_Send7bitAddress(I2C2, addr, I2C_Direction_Transmitter);
    while(!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));

    I2C_SendData(I2C2, data);
    while(!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    I2C_GenerateSTOP(I2C2, ENABLE);
}

void I2C2_Read(uint8_t addr, uint8_t *data, uint8_t size){
    I2C_GenerateSTART(I2C2, ENABLE);
    while(!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_MODE_SELECT));

    I2C_Send7bitAddress(I2C2, addr, I2C_Direction_Receiver);
    while(!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED));

    while(size){
        if(size == 1){
            I2C_AcknowledgeConfig(I2C2, DISABLE);
            I2C_GenerateSTOP(I2C2, ENABLE);
        }
				if(I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_BYTE_RECEIVED)){
            *data = I2C_ReceiveData(I2C2);
            data++;
            size--;
        }
    }
    I2C_AcknowledgeConfig(I2C2, ENABLE);
}

// ------------ UART1 -------------
void UART1_Config(void){
    USART_InitTypeDef usart;
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_USART1, ENABLE);

    // TX: PA9
    gpio.GPIO_Pin = GPIO_Pin_9;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);

    // RX: PA10
    gpio.GPIO_Pin = GPIO_Pin_10;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio);

    usart.USART_BaudRate = 9600;
    usart.USART_WordLength = USART_WordLength_8b;
    usart.USART_StopBits = USART_StopBits_1;
    usart.USART_Parity = USART_Parity_No;
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart.USART_Mode = USART_Mode_Tx | USART_Mode_Rx;
    USART_Init(USART1, &usart);

    USART_Cmd(USART1, ENABLE);
}

void UART1_SendChar(char c){
    USART_SendData(USART1, c);
    while(USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
}

int fputc(int ch, FILE *f){
    UART1_SendChar((char)ch);
    return ch;
}

// ------------ Delay -------------
void delay_ms(uint32_t ms) {
    for(uint32_t i=0; i<ms; i++) {
        for(uint32_t j=0; j<8000; j++) {
            __NOP(); // l?nh tr?ng, d? compiler không t?i uu b? vòng l?p
        }
    }
}

// ------------ MAIN -------------
int main(void){
    uint16_t light_val = 0;

    GPIO_Config_I2C2(); 
    I2C2_Config();      
    UART1_Config();     
    BH1750_Config();    

    delay_ms(180); 

    while(1){
        light_val = BH1750_GetLux();
        printf("BH1750 Lux: %u lx\r\n", light_val);
        delay_ms(500);
    }
}