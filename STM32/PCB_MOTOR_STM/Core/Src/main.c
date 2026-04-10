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
#define J_TIMEOUT 10000 // Timeout for sensor reading

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#define TRIG_PIN GPIO_PIN_4
#define TRIG_BUS GPIOB
#define ECHO_PIN GPIO_PIN_10
#define ECHO_BUS GPIOB
#define LED_PIN GPIO_PIN_1
#define LED_BUS GPIOA

#define DIST_WARN 25
#define DIST_DANGER_PCNT 40

#define BT_WATCHDOG_EN
#define METAL_DETECT_EN
#define ULTRASONIC_EN
#define MOTOR_DRIVER_EN
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
// Sensor Setup
volatile uint32_t sensor_time = 0;
volatile float distance = 1;

union {  // Last 2 bytes used for checksum
  float fnum;
  uint8_t bytes[7];
} FloatPacket; // Float bytes for distance
int checksum_index = 4; // start of checksum

volatile float sensor_time_seconds = 0;
volatile float sensor_prev_dist_green = 0;
volatile int distance_clamp = 1;
volatile bool update_flag = false; // Update on-screen value
volatile uint8_t lbar_update_flag = 0; // Update loading bar
volatile uint8_t lbar_switch_color_flag = 0;
volatile int prev_distance = 0;

#define SENS_EXP_FACTOR 0.4 // Exponential smoother

#define ADC_REF_VOLTAGE 3.3f
#define ADC_MAX_RANGE 4095
#define N_SAMPLES 100
#define ADC_THRESHOLD 2.4
// ADC Setup
volatile uint32_t adc_val = 0;
volatile float voltage = 0.0f;
volatile float volt_per_bit = ADC_REF_VOLTAGE / (float)ADC_MAX_RANGE;
static float voltage_samples[N_SAMPLES];
static int voltage_index = 0;





/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// Metal Detector Variables
volatile static uint32_t current_val = 0;
volatile static uint32_t previous_val = 0;
volatile static uint32_t delta_ticks = 1;
static float fast_delta_buf = 48.0f;
static float slow_delta_buf = 48.0f;
static char md_bypass = 0; // Used for debounce after metal trigger event
#define TRIGGER_DELTA 10 // FAST-SLOW DELTA * 100
#define FAST_DELTA_BUF_DAMP 0.2
#define SLOW_DELTA_BUF_DAMP 0.085
#define EXP_SMOOTH(prev_val,new_val,damping_factor) (prev_val * (1-damping_factor) + new_val * (damping_factor));

// Joystick Variables
volatile uint8_t rx_data[4] = {0,0,0,0};
volatile uint16_t joystick_1 = 0;
volatile uint16_t joystick_2 = 0;

volatile uint16_t joystick_1_write_val = (7 << 8);
volatile uint16_t joystick_2_write_val = (7 << 8);
volatile uint16_t joystick_curved_val;
#define MOTOR_SMOOTH 0.6 // 0.. -> 1.0 max

volatile char bt_dat_avail = 0; // Available data on bluetooth
volatile char bt_timeout = 0; // FLag for bluetooth disconnection

/**
 * @brief generate a checksum for the array given at the checksum index
 * 
 * @param arr Array to be sifted through
 * @param chksum_ind index of checksum (must allow for 2 bytes of storage on an array), if less than 0, will not store any value on the array
 * 
 */
uint16_t Checksum_Generate(uint8_t* arr, int chksum_ind) {
  // Using fletcher 16 algorithm
  uint8_t s1 = 0;
  uint8_t s2 = 0;
  char do_arr_replacement = 1;
  if(chksum_ind < 0) {
    chksum_ind = chksum_ind * -1;
    do_arr_replacement = 0;
  }
  for(int i = 0; i < chksum_ind; i++) {
    s1 += arr[i];
    s2 += s1;
  }
  if(do_arr_replacement) {
    arr[chksum_ind] = s2;
    arr[chksum_ind+1] = s1;
  }
  return ((uint16_t)s2 << 8) + (uint16_t)s1;
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
  if(htim->Instance == TIM3) { // Bluetooth timeout (watchdog)
    bt_timeout = 1;
  }
  if(htim->Instance == TIM4) { // Metal detector timer callback
    md_bypass = 0;
    FloatPacket.bytes[4] = 0;
    __HAL_TIM_SET_COMPARE(&htim12, TIM_CHANNEL_1, 0); // Turn off buzzer
    HAL_TIM_Base_Stop_IT(&htim4); // Stop debounce timer
  }
}

void BT_Watchdog() {
  if(bt_timeout) {
    bt_dat_avail = 0;
    // Turn off all motors
    __HAL_TIM_SET_COMPARE(&htim10, TIM_CHANNEL_1, 0);
    __HAL_TIM_SET_COMPARE(&htim11, TIM_CHANNEL_1, 0);
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_3, 0);
    __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_1, 0);

    HAL_UART_AbortReceive(&huart1);

    __HAL_UART_CLEAR_OREFLAG(&huart1);
    __HAL_UART_CLEAR_FEFLAG(&huart1);
    __HAL_UART_CLEAR_NEFLAG(&huart1);

    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, rx_data, sizeof(rx_data));
    __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);

    while(!bt_dat_avail) {
      // Wait until bluetooth data is received
      // printf("WATCHDOG | Attempting reconnect...\n");
      // HAL_UART_DMAStop(&huart1);
      // HAL_UART_Receive_DMA(&huart1, rx_data, 2); 
      HAL_UART_Transmit(&huart1,FloatPacket.bytes,sizeof(FloatPacket.bytes),100);
      HAL_Delay(10);

      if(huart1.RxState != HAL_UART_STATE_BUSY_RX) {
        HAL_UART_AbortReceive(&huart1);

        __HAL_UART_CLEAR_OREFLAG(&huart1);
        __HAL_UART_CLEAR_FEFLAG(&huart1);
        __HAL_UART_CLEAR_NEFLAG(&huart1);

        HAL_UARTEx_ReceiveToIdle_DMA(&huart1, rx_data, sizeof(rx_data));
        __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);
      }
    }

    // Make sure we are back in sync with data
    // memset(FloatPacket.bytes,0,4);
    // HAL_UART_Receive_DMA(&huart1, rx_data, 2); 

    __HAL_TIM_SET_COUNTER(&htim3,0); // Reset watchdog timer
    bt_timeout = 0;
  }
}

void Metal_Detect_Calibrate(int passes) {
  float frequency_poll = 84000.0f/(float)delta_ticks; // KHz
  slow_delta_buf = frequency_poll;
  int delay_val = 10;
  int bar_progress = 0;
  printf("Calibrating with %d passes (%lfs total time)\n",passes,(float)passes*delay_val/1000.0f);
  for(int i = 0; i < passes; i++) {
    frequency_poll = 84000.0f/(float)delta_ticks; // KHz
    slow_delta_buf = EXP_SMOOTH(slow_delta_buf,frequency_poll,SLOW_DELTA_BUF_DAMP);
    bar_progress = (i * 10) / passes;
    printf("[ ");
    for(int j = 0; j < bar_progress; j++) {
      printf("X");
    }
    for(int j = 0; j < (10 - bar_progress); j++) {
      printf("-");
    }
    printf(" ]\r");
    HAL_Delay(delay_val);
  }
  fast_delta_buf = slow_delta_buf;
  printf("\nCompleted calibration...\n\n");
  HAL_Delay(500);
}

void Metal_Detect_Logic() {
  float frequency_poll = 84000.0f/(float)delta_ticks; // KHz
  int sf_delta = 0;
  // if(frequency_poll < 40 || frequency_poll > 55) {
  //   return; // Invalid values
  // }
  slow_delta_buf = EXP_SMOOTH(slow_delta_buf,frequency_poll,SLOW_DELTA_BUF_DAMP);
  fast_delta_buf = EXP_SMOOTH(fast_delta_buf,frequency_poll,FAST_DELTA_BUF_DAMP);
  if(!md_bypass) {
    sf_delta = (int)(fast_delta_buf*100) - (int)(slow_delta_buf*100);


    if(sf_delta > TRIGGER_DELTA) {
      __HAL_TIM_SET_COMPARE(&htim12, TIM_CHANNEL_1, 10);
      HAL_TIM_Base_Start_IT(&htim4);
      __HAL_TIM_SET_COUNTER(&htim4,0);
      FloatPacket.bytes[4] = 1;
      md_bypass = 1;
      fast_delta_buf = slow_delta_buf = (fast_delta_buf + slow_delta_buf) / 2;
    }
  } else {
    printf("\n[ METAL HAS BEEN DETECTED!! ]");
  }
  printf("fast_delta_buf: %lf\nslow_delta_buf: %lf\ndelta: %d\n-----\n",fast_delta_buf,slow_delta_buf,sf_delta);
}

void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM5) { // Metal detector timer
        if (htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1) {

            current_val = HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);

            delta_ticks = current_val - previous_val;
            if(!delta_ticks) delta_ticks = 1; // Handle div by 0

            previous_val = current_val;
        }
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {
  if(GPIO_Pin == ECHO_PIN) {
    if(HAL_GPIO_ReadPin(ECHO_BUS,ECHO_PIN) == GPIO_PIN_SET) { // Rising edge echo
      __HAL_TIM_SET_COUNTER(&htim2,0); // Initialize clock to 0
    } else { // Falling edge echo
      sensor_time = __HAL_TIM_GET_COUNTER(&htim2);
      // Prescaler is set to 1MHz, therefore seconds = counter / 10^6
      sensor_time_seconds = sensor_time/(1000000.0f*84);

      update_flag = true;
    }
  }
}

int _write(int file, char *ptr, int len) {
  for(int i = 0; i < len; i++) {
    ITM_SendChar(*ptr++);
  }
  return len;
}

float get_avg_adc() {
  float sum = 0.0f;
  for(int i = 0; i < N_SAMPLES; i++) {
    sum += voltage_samples[i];
  }
  return sum / (float)N_SAMPLES;
}

float add_adc_val(float val) {
  voltage_samples[voltage_index] = val;
  voltage_index += 1;
  voltage_index %= N_SAMPLES;
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
	//HAL_GPIO_ReadPin(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin);
	//void HAL_GPIO_WritePin(GPIO_TypeDef* GPIOx, uint16_t GPIO_Pin, GPIO_PinState PinState);

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
  MX_SPI3_Init();
  MX_ADC1_Init();
  MX_TIM2_Init();
  MX_TIM5_Init();
  MX_TIM1_Init();
  MX_USART1_UART_Init();
  MX_TIM9_Init();
  MX_TIM8_Init();
  MX_TIM10_Init();
  MX_TIM11_Init();
  MX_TIM3_Init();
  MX_TIM12_Init();
  MX_TIM4_Init();
  /* USER CODE BEGIN 2 */

  //HAL_TIM_Base_Start(&htim5);

  // Motor PWM's
  HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_3);
  HAL_TIM_PWM_Start(&htim8, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim10, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim11, TIM_CHANNEL_1);

  HAL_TIM_PWM_Start(&htim12, TIM_CHANNEL_1); // Buzzer PWM (Base freq 100KHz / 20 (ARR) to get 2KHz PWM)
  HAL_TIM_Base_Start_IT(&htim1);
  HAL_TIM_Base_Start_IT(&htim3); // 16-bit bluetooth watchdog timer (currently configured for 1s timeout)
  HAL_TIM_IC_Start_IT(&htim5,TIM_CHANNEL_1); // 32-bit metal detector timer
  HAL_TIM_Base_Start(&htim2); // 32-bit ultrasonic timer

  // HAL_UART_Receive_DMA(&huart1, rx_data, sizeof(rx_data)); // Receive 2 bytes and store on the rx_data array

  HAL_UART_AbortReceive(&huart1);

  // 2. Clear any lingering hardware errors caused by the disconnect/static
  __HAL_UART_CLEAR_OREFLAG(&huart1);
  __HAL_UART_CLEAR_FEFLAG(&huart1);
  __HAL_UART_CLEAR_NEFLAG(&huart1);

  HAL_UARTEx_ReceiveToIdle_DMA(&huart1, FloatPacket.bytes, sizeof(FloatPacket.bytes));
  __HAL_DMA_DISABLE_IT(huart1.hdmarx, DMA_IT_HT);

  HAL_UARTEx_ReceiveToIdle_DMA(&huart1,rx_data,sizeof(rx_data));
  __HAL_DMA_DISABLE_IT(huart1.hdmarx,DMA_IT_HT);

  memset(FloatPacket.bytes,0,sizeof(FloatPacket.bytes));
  
  set_bg_col(J_BLACK);

  //fill_area(0x000F);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  // j_master_control.bgcol = J_BLACK;
  // float s, prev_s;
  // s = prev_s = 0;
  // j_counter my_counter = J_CREATE_COUNTER;
  HAL_GPIO_WritePin(LED_BUS,LED_PIN,GPIO_PIN_SET);
  HAL_GPIO_WritePin(GPIOB,GPIO_PIN_0,GPIO_PIN_SET);

  // __HAL_TIM_SET_COMPARE(&htim10, TIM_CHANNEL_1, 500);
  // __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_3, 500);

  // fill_text(120,80,"Distance (cm)",-1,J_11X18_FONT,J_WHITE,J_CENTER);

  HAL_ADC_Start(&hadc1);
  float test_num = 50;
  memcpy((void*)FloatPacket.bytes,(void*)&test_num,4); // Copy float into the Float Packet
  
  Metal_Detect_Calibrate(500);


  __HAL_TIM_SET_COMPARE(&htim12, TIM_CHANNEL_1, 10);
  HAL_Delay(1000);
  __HAL_TIM_SET_COMPARE(&htim12, TIM_CHANNEL_1, 0);
  HAL_Delay(1000);

  while(!bt_dat_avail) {
    HAL_Delay(50); // wait for first bluetooth data signal
  }
  printf("bt connection started..\n");

  while (1)

  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    __HAL_TIM_SET_COUNTER(&htim2,0);
    HAL_GPIO_WritePin(TRIG_BUS,TRIG_PIN,GPIO_PIN_SET); // Set trig pin on

    while(__HAL_TIM_GET_COUNTER(&htim2) < 840) {

    }
    HAL_GPIO_WritePin(TRIG_BUS,TRIG_PIN,GPIO_PIN_RESET); // Set trig pin off after about 1000 microseconds

    HAL_Delay(60); // Super important

    if(update_flag) { // Update for sensor (update_flag)
      update_flag = false;

      distance = (sensor_time_seconds * 34300.0f)/2.0;
      memcpy((void*)FloatPacket.bytes,(void*)&distance,4); // Copy float into the Float Packet
      Checksum_Generate(FloatPacket.bytes,sizeof(FloatPacket.bytes)-2);
      HAL_UART_Transmit(&huart1,FloatPacket.bytes,sizeof(FloatPacket.bytes),100);
      HAL_Delay(10);

      // printf("Sensor Reading: %lf cm\n", FloatPacket.fnum);
    }

    if(bt_dat_avail) {
      __HAL_TIM_SET_COUNTER(&htim3,0); // Reset watchdog timer
    	bt_dat_avail = 0;
    	joystick_1 = rx_data[0] >> 4;
    	joystick_1 = joystick_1 << 8; // Make 16 bit, higher accuracy
    	joystick_2 = rx_data[0] & 0x0F;
    	joystick_2 = joystick_2 << 8;
    	//printf("%d  %d\n", joystick_2, joystick_1);

    	joystick_1_write_val = joystick_1_write_val * MOTOR_SMOOTH + joystick_1 * (1-MOTOR_SMOOTH);
    	joystick_2_write_val = joystick_2_write_val * MOTOR_SMOOTH + joystick_2 * (1-MOTOR_SMOOTH);
    	// printf("Joystick Vals: %d  %d\nHeadlight Data: %d", joystick_2_write_val, joystick_1_write_val, rx_data[1]);
      if(rx_data[1]) {
        HAL_GPIO_WritePin(GPIOB,GPIO_PIN_7,GPIO_PIN_SET);
      } else {
        HAL_GPIO_WritePin(GPIOB,GPIO_PIN_7,GPIO_PIN_RESET);
      }


    	//__HAL_TIM_SET_COMPARE(&htim10, TIM_CHANNEL_1, 800);
    	//__HAL_TIM_SET_COMPARE(&htim11, TIM_CHANNEL_1, 0);

    	// Motor logic
    	// Motor 1

    	if(1) {
    		if(joystick_2_write_val > 2200) { // Forward Left
          //__HAL_TIM_SET_COMPARE(&htim10, TIM_CHANNEL_1, 800);
          joystick_curved_val = 400 + ((joystick_2_write_val - 2200) * 100) / 327;
          __HAL_TIM_SET_COMPARE(&htim10, TIM_CHANNEL_1, joystick_curved_val);
          __HAL_TIM_SET_COMPARE(&htim11, TIM_CHANNEL_1, 0);
        } else if(joystick_2_write_val < 1650) { // Reverse
          joystick_curved_val = 900 - (joystick_2_write_val * 10) / 33;
          __HAL_TIM_SET_COMPARE(&htim10, TIM_CHANNEL_1, 0);
          //__HAL_TIM_SET_COMPARE(&htim11, TIM_CHANNEL_1, 800);
          __HAL_TIM_SET_COMPARE(&htim11, TIM_CHANNEL_1, joystick_curved_val);
        } else { // Stop
          __HAL_TIM_SET_COMPARE(&htim10, TIM_CHANNEL_1, 0);
          __HAL_TIM_SET_COMPARE(&htim11, TIM_CHANNEL_1, 0);
        }
    	}

    	// Motor 2 Right
    	if(1) {
    		if(joystick_1_write_val > 1900) { // Forward
          joystick_curved_val = 400 + ((joystick_1_write_val - 1900) * 10) / 43;
          __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_3, joystick_curved_val);
          __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_1, 0);
        } else if(joystick_1_write_val < 1600) { // Reverse
          joystick_curved_val = 900 - (joystick_1_write_val * 5) / 16;
          __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_3, 0);
          __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_1, joystick_curved_val);
        } else { // Stop
          __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_3, 0);
          __HAL_TIM_SET_COMPARE(&htim8, TIM_CHANNEL_1, 0);
        }
    	}
    }

    BT_Watchdog();
    Metal_Detect_Logic();
    HAL_Delay(1);
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
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
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

// void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {

//     if (huart->Instance == USART1) {
//         bt_dat_avail = 1; // Let the code know we have new data ready
//     }
// }

void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
  if(huart->Instance == USART1) {
    int rx_size = sizeof(rx_data);
    uint16_t checksum = Checksum_Generate(rx_data,-1*(rx_size-2));
    if(checksum == ((rx_data[rx_size-2] << 8) + rx_data[rx_size-1])) {
      bt_dat_avail = 1;
      HAL_UARTEx_ReceiveToIdle_DMA(&huart1,rx_data,rx_size);
    } else {
      for(int i = 0; i < rx_size-1; i++) {
        rx_data[i] = rx_data[i+1];
      }
      HAL_UARTEx_ReceiveToIdle_DMA(&huart1,&rx_data[rx_size-1],1);
    }
    __HAL_DMA_DISABLE_IT(huart->hdmarx,DMA_IT_HT);
  }

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
