/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Combined: ADXL345 + NanoEdgeAI classification + UART to ESP32
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "NanoEdgeAI.h"

/* Private defines -----------------------------------------------------------*/
#define NB_AXES      3
#define BUFFER_SIZE  256

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;
UART_HandleTypeDef huart1;   // to ESP32
UART_HandleTypeDef huart2;   // debug / console

/* USER CODE BEGIN PV */
float acc_buffer[NB_AXES * BUFFER_SIZE];
uint8_t data_rec[6];
uint8_t adxl_addr = 0x53 << 1;

/* Mapping class IDs to names */
const char* id2class[3] = {"Good", "Bad", "Worst"};
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);

void adxl_init(void);
void adxl_write(uint8_t reg, uint8_t value);
void adxl_read(uint8_t reg, uint8_t numberofbytes);

/* USER CODE BEGIN 0 */
void adxl_write(uint8_t reg, uint8_t value) {
    uint8_t data[2] = {reg, value};
    HAL_I2C_Master_Transmit(&hi2c1, adxl_addr, data, 2, 10);
}

void adxl_read(uint8_t reg, uint8_t numberofbytes) {
    HAL_I2C_Mem_Read(&hi2c1, adxl_addr, reg, 1, data_rec, numberofbytes, 100);
}

void adxl_init(void) {
    adxl_write(0x2D, 0x08);   // power control: measure mode
}

/* Redirect printf to UART2 (debug) */
int __io_putchar(int ch) {
    HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}
/* USER CODE END 0 */

int main(void) {
    HAL_Init();
    SystemClock_Config();

    MX_GPIO_Init();
    MX_I2C1_Init();
    MX_USART1_UART_Init();   // <-- added: UART for ESP32
    MX_USART2_UART_Init();   // debug UART

    /* USER CODE BEGIN 2 */
    adxl_init();

    if (neai_classification_init() != NEAI_OK) {
        printf("Init_Error\r\n");
    }
    /* USER CODE END 2 */

    while (1) {

        /* 1. Collect BUFFER_SIZE samples from ADXL345 */
        for (uint16_t i = 0; i < BUFFER_SIZE; i++) {
            uint8_t status = 0;
            do {
                HAL_I2C_Mem_Read(&hi2c1, adxl_addr, 0x30, 1, &status, 1, 10);
            } while (!(status & 0x80));   // wait for new data

            adxl_read(0x32, 6);            // read X,Y,Z raw data (2 bytes each)

            int16_t raw_x = (int16_t)(data_rec[1] << 8 | data_rec[0]);
            int16_t raw_y = (int16_t)(data_rec[3] << 8 | data_rec[2]);
            int16_t raw_z = (int16_t)(data_rec[5] << 8 | data_rec[4]);

            // convert to g (scale factor for ±16g range)
            acc_buffer[NB_AXES * i]     = (float)raw_x * 0.0039f;
            acc_buffer[NB_AXES * i + 1] = (float)raw_y * 0.0039f;
            acc_buffer[NB_AXES * i + 2] = (float)raw_z * 0.0039f;
        }

        /* 2. Run NanoEdgeAI classification */
        float probabilities[3];
        int id_class = 0;
        neai_classification(acc_buffer, probabilities, &id_class);

        /* 3. Send result to ESP32 (UART1) and also print on debug UART2 */
        if (id_class >= 1 && id_class <= 3) {
            const char* class_name = id2class[id_class - 1];

            // Send to ESP32 via UART1 (add newline for easy parsing)
            char msg[32];
            int len = sprintf(msg, "%s\r\n", class_name);
            HAL_UART_Transmit(&huart1, (uint8_t*)msg, len, 100);

            // Also print on debug console (UART2)
            printf("%s\r\n", class_name);
        }

        HAL_Delay(10);   // small delay before next inference
    }
}

/* --- System configuration functions (kept from first code) --- */
void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
    RCC_OscInitStruct.HSIState = RCC_HSI_ON;
    RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
    RCC_OscInitStruct.PLL.PLLM = 8;
    RCC_OscInitStruct.PLL.PLLN = 84;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 4;
    HAL_RCC_OscConfig(&RCC_OscInitStruct);
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2);
}

static void MX_I2C1_Init(void) {
    hi2c1.Instance = I2C1;
    hi2c1.Init.ClockSpeed = 100000;
    hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
    hi2c1.Init.OwnAddress1 = 0;
    hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c1.Init.OwnAddress2 = 0;
    hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    HAL_I2C_Init(&hi2c1);
}

static void MX_USART1_UART_Init(void) {
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart1);
}

static void MX_USART2_UART_Init(void) {
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart2);
}

static void MX_GPIO_Init(void) {
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();
}

void Error_Handler(void) {
    __disable_irq();
    while (1) {}
}
