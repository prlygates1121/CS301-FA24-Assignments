/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
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
#include "gpio.h"
#include "lcd_v4.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define ENCODE 0
#define DECODE 1
#define KEY0_PRESSED (HAL_GPIO_ReadPin(KEY0_GPIO_Port, KEY0_Pin) == GPIO_PIN_RESET)
#define KEY1_PRESSED (HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin) == GPIO_PIN_RESET)
#define KEY_WAKEUP_PRESSED (HAL_GPIO_ReadPin(KEY_WAKEUP_GPIO_Port, KEY_WAKEUP_Pin) == GPIO_PIN_SET)
#define KEY_WAKEUP_VAL 1
#define KEY0_VAL 2
#define KEY1_VAL 3
#define KEY0_KEY1_VAL 4

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#define BIT_READ(arr, bit)         ((arr[(bit) >> 3] >> ((bit) % 8)) & 1)
#define BIT_SET(arr, bit)          (arr[(bit) >> 3] |= (1 << ((bit) % 8)))
#define BIT_CLEAR(arr, bit)        (arr[(bit) >> 3] &= ~(1 << ((bit) % 8)))
#define BIT_TOGGLE(arr, bit)       (arr[(bit) >> 3] ^= (1 << ((bit) % 8)))
#define BIT_COPY(arr, bit, copy)   \
	do { \
		if (copy) { \
			BIT_SET(arr, bit); \
		} else { \
			BIT_CLEAR(arr, bit); \
		} \
	} while (0);
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
extern const unsigned char decodeSuccess[14160];
const uint32_t interval_millis = 500;
uint32_t current_millis = 0;
char* hamming[] = {"Hamming(3,1)", "Hamming(5,2)", "Hamming(6,3)", "Hamming(7,4)",
		"Hamming(9,5)", "Hamming(10,6)", "Hamming(11,7)", "Hamming(12,8)",
		"Hamming(13,9)", "Hamming(14,10)", "Hamming(15,11)"};
uint8_t hamming_version = 8;
uint8_t original_data_iterator = 0, encoded_data_iterator = 0;
uint8_t input_complete = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void lcd_show_image(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const unsigned char image[]) {
	uint16_t pointer = 0;
	for (uint16_t yy = y; yy < y + height; yy++) {
		for (uint16_t xx = x; xx < x + width; xx++) {
			uint16_t color = ((image[2 * pointer + 1] << 8) | image[2 * pointer]);
			pointer++;
			lcd_draw_point(xx, yy, color);
		}
	}
}
uint8_t key_scan() {
	static uint8_t key_up = 1;
	static uint8_t double_pressed = 0;
	uint8_t key_val = 0;

	if (key_up && (KEY0_PRESSED || KEY1_PRESSED || KEY_WAKEUP_PRESSED)) {
		HAL_Delay(10);
		if (KEY_WAKEUP_PRESSED) {
			key_val = KEY_WAKEUP_VAL;
		} else if (KEY0_PRESSED) {
			key_val = KEY0_VAL;
		} else if (KEY1_PRESSED) {
			key_val = KEY1_VAL;
		}
		key_up = 0;
		double_pressed = 0;
	} else if (!KEY0_PRESSED && !KEY1_PRESSED && !KEY_WAKEUP_PRESSED) {
		key_up = 1;
		double_pressed = 0;
	} else if (!double_pressed && (KEY0_PRESSED || KEY1_PRESSED)) {
		HAL_Delay(10);
		if (KEY0_PRESSED && KEY1_PRESSED) {
			key_val = KEY0_KEY1_VAL;
			double_pressed = 1;
		} else {
			double_pressed = 0;
		}
	}

	return key_val;
}

void flash_LEDs(uint8_t times) {
	for (uint8_t i = 0; i < times << 1; i++) {
	  HAL_GPIO_TogglePin(LED0_GPIO_Port, LED0_Pin);
	  HAL_GPIO_TogglePin(LED1_GPIO_Port, LED1_Pin);
	  HAL_Delay(300);
	}
}

void init(uint8_t mode) {
	input_complete = 0;
	lcd_clear(WHITE);
	lcd_show_string(30, 30, 100, 24, 24, "MODE:", BLACK);
	if (mode == ENCODE) {
	  original_data_iterator = 0;
	  lcd_show_string(110, 30, 100, 24, 24, "ENCODE", BLUE);
	  lcd_show_string(30, 70, 200, 12, 12, "Original Data:", BLACK);
	} else { // if mode is switched to DECODE, LEDs flash once
	  encoded_data_iterator = 0;
	  lcd_show_string(110, 30, 100, 24, 24, "DECODE", GREEN);
	  lcd_show_string(30, 70, 200, 12, 12, "Encoded Data:", BLACK);
	}
	lcd_show_string(115, 290, 140, 16, 16, hamming[hamming_version - 1], BLACK);
}
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
  lcd_init();
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  /* USER CODE BEGIN 2 */
  uint8_t mode = 0;
  uint8_t encoding = 0, decoding = 0;
  uint8_t data_bits = 8;
  uint8_t data[2];
  uint8_t parity_bits = 4;
  uint8_t encode_bits = data_bits + parity_bits;
  uint8_t encode[2];
  uint8_t check[1];
  uint8_t underscore = 0;
  uint8_t after_input = 0;
  g_back_color = WHITE;
  init(mode);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  // Real-time update of input
	  if (mode == ENCODE) {
		  if (input_complete == 0) {
			  if (original_data_iterator == data_bits) {
				  input_complete = 1;
			  }
			  uint8_t x_offset = 0;
			  for (uint8_t i = 0; i < original_data_iterator; i++) {
				  lcd_show_num(30 + x_offset, 85, BIT_READ(data, i), 1, 24, BLACK);
				  x_offset += 12;
			  }
			  if (original_data_iterator < data_bits) {
				  if (after_input) {
					  lcd_show_char(30 + x_offset, 85, '_', 24, 0, BLACK);
					  current_millis = HAL_GetTick();
					  after_input = 0;
				  }
				  if (HAL_GetTick() - current_millis >= interval_millis) {
					  if (underscore) {
						  lcd_show_char(30 + x_offset, 85, '_', 24, 0, BLACK);
					  } else {
						  lcd_show_char(30 + x_offset, 85, ' ', 24, 0, BLACK);
					  }
					  underscore ^= 1;
					  current_millis = HAL_GetTick();
				  }
			  }
		  }
	  } else {
		  if (input_complete == 0) {
			  if (encoded_data_iterator == encode_bits) {
				  input_complete = 1;
			  }
			  uint8_t x_offset = 0;
			  for (uint8_t i = 0; i < encoded_data_iterator; i++) {
				  lcd_show_num(30 + x_offset, 85, BIT_READ(encode, i), 1, 24, BLACK);
				  x_offset += 12;
			  }
			  if (encoded_data_iterator < encode_bits) {
				  if (after_input) {
					  lcd_show_char(30 + x_offset, 85, '_', 24, 0, BLACK);
					  current_millis = HAL_GetTick();
					  after_input = 0;
				  }
				  if (HAL_GetTick() - current_millis >= interval_millis) {
					  if (underscore) {
						  lcd_show_char(30 + x_offset, 85, '_', 24, 0, BLACK);
					  } else {
						  lcd_show_char(30 + x_offset, 85, ' ', 24, 0, BLACK);
					  }
					  underscore ^= 1;
					  current_millis = HAL_GetTick();
				  }
			  }
		  }
	  }

	  volatile uint8_t key_val = key_scan();
	  // Detect key
	  if (key_val) {
		  switch (key_val) {
		  	  case KEY_WAKEUP_VAL:
				  mode = !mode;
				  init(mode);
				  if (mode == ENCODE) {
					  flash_LEDs(2);
				  } else {
					  flash_LEDs(1);
				  }
				  break;
		  	  case KEY0_VAL:
		  		  after_input = 1;
		  		  if (mode == ENCODE) {
		  			  if (original_data_iterator < data_bits) {
		  				  BIT_CLEAR(data, original_data_iterator);
		  				  original_data_iterator++;
		  				  if (original_data_iterator == data_bits) {
		  			  		  encoding = 1;
		  				  }
		  			  } else {
		  				  init(mode);
		  			  }
		  		  } else {
		  			  if (encoded_data_iterator < encode_bits) {
		  				  BIT_CLEAR(encode, encoded_data_iterator);
		  				  encoded_data_iterator++;
		  				  if (encoded_data_iterator == encode_bits) {
		  			  		  decoding = 1;
		  				  }
		  			  } else {
		  				  init(mode);
		  			  }
		  		  }
		  		  break;
		  	  case KEY1_VAL:
		  		  after_input = 1;
		  		if (mode == ENCODE) {
					  if (original_data_iterator < data_bits) {
						  BIT_SET(data, original_data_iterator);
						  original_data_iterator++;
						  if (original_data_iterator == data_bits) {
							  encoding = 1;
						  }
					  } else {
						  init(mode);
					  }
				  } else {
					  if (encoded_data_iterator < encode_bits) {
						  BIT_SET(encode, encoded_data_iterator);
						  encoded_data_iterator++;
						  if (encoded_data_iterator == encode_bits) {
							  decoding = 1;
						  }
					  } else {
						  init(mode);
					  }
				  }
		  		  break;
		  	  case KEY0_KEY1_VAL:
		  		  hamming_version = hamming_version == 11 ? 1 : hamming_version + 1;
		  		  data_bits = hamming_version;
		  		  parity_bits = data_bits == 1 ? 2 : data_bits <= 4 ? 3 : 4;
		  		  encode_bits = data_bits + parity_bits;
		  		  init(mode);
		  	  default:
		  }
	  } else {
		  HAL_Delay(10);
	  }
	  
	  // Encoding process (only executed once while mode == ENCODE)
	  if (mode == ENCODE && encoding) {
		  uint8_t data_idx = data_bits - 1;
		  for (int8_t i = encode_bits - 1; i >= 0; i--) {
			  // if i + 1 is not a power of 2
			  if (((i + 1) & i) != 0) {
				  BIT_COPY(encode, i, BIT_READ(data, data_idx));
				  data_idx--;
			  } else { // if i + 1 is a power of 2
				  uint8_t parity_bit = 0;
				  BIT_CLEAR(encode, i);
				  for (uint8_t j = i; j < encode_bits; j += ((i + 1) << 1)) {
					  for (uint8_t k = 0; k < i + 1 && j + k < encode_bits; k++) {
						  parity_bit ^= BIT_READ(encode, j + k);
					  }
				  }
				  BIT_COPY(encode, i, parity_bit);
			  }
		  }
		  // Print encoded data
		  lcd_show_string(30, 120, 200, 12, 12, "Encoded Data:", BLACK);
		  for (uint8_t i = 0, x_offset = 0; i < encode_bits; i++) {
			  if (((i + 1) & i) != 0) {
				  lcd_show_num(30 + x_offset, 135, BIT_READ(encode, i), 1 , 24, BLACK);
			  } else {
				  lcd_show_num(30 + x_offset, 135, BIT_READ(encode, i), 1 , 24, BLUE);
			  }
			  x_offset += 12;
		  }
		  encoding = 0;
	  }

	  // Decoding process (only executed once while mode == DECODE)
	  if (mode == DECODE && decoding) {
		  uint8_t check_idx = 0;
		  uint8_t incorrect = 0;
		  uint8_t bad_bit = 0;
		  for (int8_t i = encode_bits - 1; i >= 0; i--) {
			  if (((i + 1) & i) == 0) {
				  uint8_t check_bit = 0;
				  for (uint8_t j = i; j < encode_bits; j += ((i + 1) << 1)) {
					  for (uint8_t k = 0; k < i + 1 && j + k < encode_bits; k++) {
						  check_bit ^= BIT_READ(encode, j + k);
					  }
				  }
				  // check_bit == 0 means that even parity is achieved
				  // put check_bit in check
				  BIT_COPY(check, check_idx, check_bit);
				  incorrect |= check_bit;
				  check_idx++;
			  }
		  }
		  // Detect errors & find bad_bit
		  for (uint8_t i = 0; i < parity_bits; i++) {
			  bad_bit |= BIT_READ(check, i);
			  bad_bit = bad_bit << 1;
		  }
		  bad_bit = bad_bit >> 1;
		  // Print decoded data
		  lcd_show_string(30, 120, 200, 12, 12, "Decoded Data:", BLACK);
		  for (uint8_t i = 0, x_offset = 0; i < encode_bits; i++) {
			  if (((i + 1) & i) != 0) {
				  if (i + 1 == bad_bit) {
					  lcd_show_num(30 + x_offset, 135, BIT_READ(encode, i), 1, 24, RED);
				  } else {
					  lcd_show_num(30 + x_offset, 135, BIT_READ(encode, i), 1, 24, BLACK);
				  }
				  x_offset += 12;
			  }
		  }
		  // Print the number of parity bits, and the parity bits themselves
		  lcd_show_string(30, 170, 200, 12, 12, "Total Parity Bits:", BLACK);
		  lcd_show_num(30, 185, parity_bits, 1, 24, BLACK);
		  lcd_show_char(54, 185, '[', 24, 1, BLACK);
		  for (uint8_t i = 0, x_offset = 0; i < parity_bits; i++, x_offset += 12) {
			  lcd_show_num(66 + x_offset, 185, BIT_READ(check, i), 1, 24, BLACK);
			  if (i == parity_bits - 1) {
				  lcd_show_char(66 + x_offset + 12, 185, ']', 24, 1, BLACK);
			  }
		  }
		  // Print error information
		  if (bad_bit > encode_bits) {
			  lcd_show_string(30, 220, 200, 16, 16, "Multiple errors occurred", RED);
		  } else {
			  lcd_show_image(140, 170, 100, 49, decodeSuccess);
			  if (incorrect) {
				  lcd_show_string(30, 220, 200, 16, 16, "Error occurred at bit", RED);
				  lcd_show_num(205, 220, bad_bit, bad_bit >= 100 ? 3 : bad_bit >= 10 ? 2 : 1, 16, RED);
				  // Print the corrected data if one of the data bits is incorrect
				  if (((bad_bit) & (bad_bit - 1)) != 0) {
					  lcd_show_string(30, 240, 200, 12, 12, "Corrected Data:", BLACK);
					  for (uint8_t i = 0, x_offset = 0; i < encode_bits; i++) {
						  if (((i + 1) & i) != 0) {
							  if (i + 1 == bad_bit) {
								  lcd_show_num(30 + x_offset, 255, !BIT_READ(encode, i), 1 , 24, BLACK);
							  } else {
								  lcd_show_num(30 + x_offset, 255, BIT_READ(encode, i), 1 , 24, BLACK);
							  }
							  x_offset += 12;
						  }
					  }
				  } else {
					  lcd_show_string(30, 240, 200, 16, 16, "(which is a parity bit)", RED);
				  }
			  } else {
				  lcd_show_string(30, 220, 200, 16, 16, "No error occurred", GREEN);
			  }
		  }
		  decoding = 0;
	  }


	  // Route KEY0 and KEY1 to LED0 and LED1 respectively
	  if (KEY0_PRESSED) { // KEY0 is pressed
		  HAL_GPIO_WritePin(LED0_GPIO_Port, LED0_Pin, GPIO_PIN_RESET); // LED0 is turned on
	  } else {
		  HAL_GPIO_WritePin(LED0_GPIO_Port, LED0_Pin, GPIO_PIN_SET);
	  }
	  if (KEY1_PRESSED) { // KEY1 is pressed
		  HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_RESET); // LED1 is turned on
	  } else {
		  HAL_GPIO_WritePin(LED1_GPIO_Port, LED1_Pin, GPIO_PIN_SET);
	  }


    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
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

#ifdef  USE_FULL_ASSERT
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
