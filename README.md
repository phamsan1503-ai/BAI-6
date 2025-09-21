# Bài 6 - Giao tiếp I2C

## Nội dung bài tập

_Bài tập yêu cầu:_

- Cấu hình STM32 là Master I2C

- Giao tiếp với một EEPROM hoặc cảm biến I2C để đọc/ghi dữ liệu.

- Hiển thị dữ liệu đọc được lên màn hình terminal qua UART


***File code kết quả: [Link](https://github.com/phamsan1503-ai/BAI-5)***


### 1. Cấu hình STM32 là Master I2C 

- Cấu hình PB10 (SCL), PB11 (SDA) cho I2C2.

- Kiểu: Alternate Function Open-Drain, tốc độ 50MHz
  
👉 Mục đích: chuẩn bị chân GPIO cho giao tiếp I2C.
```c
void GPIO_Config_I2C2(void){
    GPIO_InitTypeDef gpio;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    gpio.GPIO_Pin   = GPIO_Pin_10 | GPIO_Pin_11;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode  = GPIO_Mode_AF_OD;
    GPIO_Init(GPIOB, &gpio);
}
```
- Bật clock cho I2C2.

- Thiết lập chuẩn I2C:

- Tốc độ: 100kHz.

- 7-bit address.

- Cho phép ACK.

- Enable I2C2.

👉 Mục đích: khởi tạo phần cứng I2C2 của STM32.

```c
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
```
### 2. Giao tiếp với cảm biến ánh sáng Lux BH1750

- Gửi lệnh khởi tạo cảm biến BH1750 qua I2C:

- 0x01: Power ON.

- 0x07: Reset.

- 0x10: Chế độ đo liên tục độ phân giải cao (1 lx, chu kỳ ~120ms).

👉 Mục đích: bật và cấu hình chế độ hoạt động cho BH1750.
```c
void BH1750_Config(void){
    uint8_t lenh [] = {0x01, 0x07, 0x10}; 
    for (uint8_t i = 0; i < sizeof(lenh); i++) {
        I2C2_Write(BH1750_ADDR, lenh[i]);
    }
}
```

- Đọc 2 byte dữ liệu ánh sáng từ BH1750.

- Ghép thành 16-bit (MSB << 8 | LSB).

- Chia cho 1.2 (làm tròn thành nhân 5/6) để đổi ra đơn vị lux.

👉 Mục đích: lấy giá trị độ sáng từ cảm biến.
```c
	uint16_t BH1750_GetLux(void){
    uint8_t raw_data[2];
    uint16_t raw_value;

    I2C2_Read(BH1750_ADDR, raw_data, 2);
    raw_value = (raw_data[0] << 8) | raw_data[1];

    return (raw_value * 5) / 6; // 1/1.2 ˜ 5/6
}
```

- Tạo START.

- Gửi địa chỉ addr (slave) với hướng ghi.

- Gửi 1 byte dữ liệu.

- STOP.

👉 Mục đích: ghi 1 byte vào cảm biến BH1750.

```c
	void I2C2_Write(uint8_t addr, uint8_t data){
    I2C_GenerateSTART(I2C2, ENABLE);
    while(!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_MODE_SELECT));

    I2C_Send7bitAddress(I2C2, addr, I2C_Direction_Transmitter);
    while(!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED));

    I2C_SendData(I2C2, data);
    while(!I2C_CheckEvent(I2C2, I2C_EVENT_MASTER_BYTE_TRANSMITTED));

    I2C_GenerateSTOP(I2C2, ENABLE);
}
```

- Tạo START.

- Gửi địa chỉ addr với hướng đọc.

- Đọc tuần tự size byte dữ liệu.

- Trước byte cuối: tắt ACK và phát STOP.

👉 Mục đích: đọc nhiều byte dữ liệu từ cảm biến.
```c
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
```

- Khởi tạo GPIO, I2C2, UART1, và BH1750.
  
- Chờ khởi động cảm biến.

- Vòng lặp chính:

- Đọc lux từ BH1750.

- In ra UART (terminal).

- Delay 500ms.
```c
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
```
***Video demo kết quả: [Link](https://drive.google.com/file/d/1AtWIWcqCZ4q288yya1_UmL2BXS0-tYjG/view?usp=drive_link)***




```
***Video  kết quả: [Link](https://drive.google.com/drive/folders/1pxEtEL510zTSKQkZHq40Ss9nMOm-HtgT?usp=drive_link)***
