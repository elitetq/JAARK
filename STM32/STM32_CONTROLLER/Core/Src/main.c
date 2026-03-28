/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "j_st7789.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
// Slave address 2025:08:004B82

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
// uint8_t rx_buf[1]; // Data we receive
// uint8_t tx_buf[1]; // Data to transmit
uint8_t pc_rx_buf[1];
uint8_t hc_rx_buf[1];

volatile union { 
  float fnum;
  uint8_t bytes[6];
} FloatPacket; // Float bytes for distance
volatile float fnum_stored = 0;
volatile char bt_dat_avail = 0;
volatile char bt_timeout = 0;
volatile uint32_t hal_adc_val = 0; // X
volatile uint32_t hal_adc_val2 = 0; // Y
volatile uint8_t hal_combined[2] = {0,0}; // index 1 {Joystick Data} : index 2 {Headlight Data}


#define BT_BUF_SIZE 30
#define BG_COL J_BLACK
#define WATCHDOG_BG_COL J_RED

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

void Reset_DMA_Channel(UART_HandleTypeDef *huart, uint8_t *pData, uint8_t size) {
}

void Setup_Main_Screen() {
  set_bounds(0,239,0,239);
  set_bg_col(BG_COL);
  fill_area(get_bg_col());

  fill_text(120,90,"Distance (cm)",13,J_11X18_FONT,J_WHITE,J_CENTER);
}

void BT_Watchdog() {
  if(bt_timeout) {
    bt_dat_avail = 0;
    set_bounds(0,239,0,239);
    set_bg_col(WATCHDOG_BG_COL);
    fill_area(get_bg_col());

    fill_text(120,75,"WATCHDOG",-1,J_16X26_FONT,J_WHITE,J_CENTER);
    fill_text(120,102,"No Bluetooth Signal",-1,J_11X18_FONT,J_WHITE,J_CENTER);
    fill_text(120,125,"Attempting Reconnect...",-1,J_7X10_FONT,J_WHITE,J_CENTER);

    HAL_Delay(15);

    uint8_t rec_index1, rec_index2, rec_flag1, rec_flag2, x_pos, y_pos, width;
    rec_index2 = 0;
    rec_index1 = 1;
    rec_flag1 = 0;
    rec_flag2 = 0;
    x_pos = 100;
    y_pos = 165;
    width = 20;


    while(!bt_dat_avail) {
      printf("WATCHDOG | Attempting reconnect...");
      // Wait until bluetooth data is received
      set_bounds(x_pos+rec_flag1*width,x_pos+width*(1+rec_flag1),y_pos+rec_flag2*width,y_pos+width*(1+rec_flag2)); // Loading bar
      fill_area(J_WHITE);
      // HAL_UART_Receive_DMA(&huart1,FloatPacket.bytes,4); // Receive ultrasonic data
      HAL_UART_DMAStop(&huart1); 
      HAL_UART_Receive_DMA(&huart1,FloatPacket.bytes,sizeof(FloatPacket));
      HAL_UART_Transmit(&huart1,hal_combined,2,100);

      HAL_Delay(50);

      fill_area(get_bg_col());
      rec_index1++;
      rec_index2++;
      if(rec_index1 >= 2) {
        rec_flag1 = !rec_flag1;
        rec_index1 %= 2;
      }
      if(rec_index2 >= 2) {
        rec_flag2 = !rec_flag2;
        rec_index2 %= 2;
      }
    }
    Setup_Main_Screen();

    // Make sure we are back in sync with data
    HAL_UART_DMAStop(&huart1); // A timeout means unexpected drop in bt connection, we need to disable dma in case of data corruption
    HAL_Delay(100);
    memset(FloatPacket.bytes,0,6);
    HAL_UART_Receive_DMA(&huart1,FloatPacket.bytes,sizeof(FloatPacket));

    __HAL_TIM_SET_COUNTER(&htim3,0); // Reset watchdog timer
    bt_timeout = 0;
  }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
  if(htim->Instance == TIM3) {
    bt_timeout = 1;
  }
}

int _write(int file, char *ptr, int len) {
  for(int i = 0; i < len; i++) {
    ITM_SendChar(*ptr++);
  }
  return len;
}
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC2_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_ADC1_Init();
  MX_TIM3_Init();
  MX_SPI2_Init();
  /* USER CODE BEGIN 2 */
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  GPIO_InitStruct.Pin = GPIO_PIN_10;
  GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;   // Alternate Function Push-Pull
  GPIO_InitStruct.Pull = GPIO_PULLUP;      // This prevents the "float"
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
  GPIO_InitStruct.Alternate = GPIO_AF7_USART1; // AF7 is standard for UART on PA10
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
  // HAL_UART_Receive_IT(&huart2, pc_rx_buf, 1);
  // HAL_UART_Receive_IT(&huart1, hc_rx_buf, 1);
  HAL_UART_Receive_DMA(&huart1,FloatPacket.bytes,sizeof(FloatPacket)); // Receive ultrasonic data

  uint8_t cmd_at[] = "AT\r\n";
  uint8_t cmd_role[] = "AT+ADDR?\r\n";  // Setting to Slave
  uint8_t rx_buffer[BT_BUF_SIZE] = {0};

  HAL_TIM_Base_Start_IT(&htim3); // 16-bit bluetooth watchdog timer (currently configured for 1s timeout)


  // HAL_UART_Transmit(&huart2, (uint8_t*)"\r\n--- Sending AT+ROLE=0 ---\r\n", 29, 100);
  // HAL_UART_Transmit(&huart1, cmd_role, strlen((char*)cmd_role), 100);
  
  
  // HAL_UART_Receive(&huart1, rx_buffer, sizeof(rx_buffer), 500);
  // HAL_UART_Transmit(&huart2, rx_buffer, strlen((char*)rx_buffer), 100);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  init_lcd(J_270_DEG);
  HAL_Delay(50);
  set_bg_col(J_RED);
  set_bounds(0,239,0,239);
  fill_area(get_bg_col());
  HAL_Delay(100);
  // fill_text(120,120,"Hello World",-1,J_11X18_FONT,J_WHITE,J_CENTER);

  j_counter my_counter = J_CREATE_COUNTER;



  HAL_GPIO_WritePin(GPIOB,GPIO_PIN_12,GPIO_PIN_SET);
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1,1000);
    hal_adc_val = HAL_ADC_GetValue(&hadc1);

    HAL_ADC_Start(&hadc2);
    HAL_ADC_PollForConversion(&hadc2,1000);
    hal_adc_val2 = HAL_ADC_GetValue(&hadc2);

    hal_adc_val = (hal_adc_val << 4) >> 12; // cnv from 12-bit to 4-bit
    hal_adc_val2 = (hal_adc_val2 << 4) >> 12;

    hal_combined[0] = ((hal_adc_val << 4) | (hal_adc_val2));
    //hal_combined[0] = 0xF0;

    if(!(HAL_GPIO_ReadPin(GPIOC,GPIO_PIN_3) && HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_4))) {
      hal_combined[1] = 1; // Headlight data
    } else {
      hal_combined[1] = 0; 
    }

    HAL_UART_Transmit(&huart1,hal_combined,2,100);

    if(bt_dat_avail) {
      bt_dat_avail = 0;
      __HAL_TIM_SET_COUNTER(&htim3,0); // Reset watchdog timer
      // printf("Distance received: [%d %d %d %d] %lf\n",FloatPacket.bytes[0],
      // FloatPacket.bytes[1],
      // FloatPacket.bytes[2],
      // FloatPacket.bytes[3], FloatPacket.fnum);
      printf("Data received: %lf", fnum_stored);
      // Writing to LCD
      fill_counter(&my_counter,120,120,fnum_stored,J_16X26_FONT,J_WHITE,J_CENTER);

    }

    // printf("Joystick Vals: %d %d\nHeadlight Val: %d\n\n",hal_adc_val,hal_adc_val2,hal_combined[1]);
  

    HAL_Delay(10);

    BT_Watchdog();
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 82;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

void Send_HC_AT_Command(char *str, uint8_t *rx_buf) {
  int i = 0;
  while(str[i]) { // Get length
    i++;
  }
  char local_str[64];
  memset(local_str,0,sizeof(local_str));
  
  strcpy(local_str,"-- Sending -> ");
  if(i <= 40)
    strcat(local_str,str);

  HAL_UART_Transmit(&huart2, (uint8_t*)local_str, strlen(local_str), 100);
  HAL_UART_Transmit(&huart1, (uint8_t*)str, i, 100);

  memset(rx_buf, 0, BT_BUF_SIZE);
  
  HAL_UART_Receive(&huart1, rx_buf, BT_BUF_SIZE, 500);
  HAL_UART_Transmit(&huart2, rx_buf, BT_BUF_SIZE, 100);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
	// printf("Entered callback\n");
  // If the byte came from the PC (USART2)
  // if (huart->Instance == USART2) 
  // {
	// printf("USART2\n");
  //   // Forward it to the HC-05 (USART1)
  //   HAL_UART_Transmit(&huart1, pc_rx_buf, 1, 10);
  //   // Restart the listener for the next PC keystroke
  //   HAL_UART_Receive_IT(&huart2, pc_rx_buf, 1);
  // }
  
  // If the byte came from the HC-05 (USART1)
  if (huart->Instance == USART1) 
  {
    bt_dat_avail = 1; // Let main know we have new bluetooth data
    fnum_stored = FloatPacket.fnum;
	// printf("USART1\n");
  //   HAL_GPIO_TogglePin(GPIOA, GPIO_PIN_5);
  //   // Forward it to the PC (USART2)
  //   HAL_UART_Transmit(&huart2, hc_rx_buf, 1, 10);
  //   // Restart the listener for the next HC-05 response
  //   HAL_UART_Receive_IT(&huart1, hc_rx_buf, 1);
  }
}


void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    
    // if(huart->Instance == USART2)
    // {
    //     HAL_UART_Receive_IT(&huart2, pc_rx_buf, 1);
    // }
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
