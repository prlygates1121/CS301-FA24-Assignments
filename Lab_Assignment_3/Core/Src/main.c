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
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "lcd_v4.h"
#include "images.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
extern TIM_HandleTypeDef htim3;
extern UART_HandleTypeDef huart1;
extern uint8_t img[IMG_SIZE];
extern uint8_t frame_ready;
extern uint8_t update_time;
extern uint8_t rxBuffer[20];
extern uint8_t video_status;

uint8_t img_copy[IMG_SIZE];
uint8_t capture_frame = 0;
uint16_t frame_index = -1;
uint16_t frame_total = 0;
uint8_t scale = 1;

uint8_t update_button = 0;
uint8_t sel_button = 0;

uint16_t sec_count = 0;

uint8_t update_playback_speed = 0;
uint8_t sel_playback_speed = 0;

const uint8_t signal[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void lcd_show_progress_bar() {
	lcd_fill(30, 175, 30 + 180 * ((double)(frame_index + 1) / frame_total), 180, BLACK);
}

void lcd_show_image(uint16_t x, uint16_t y, uint16_t width, uint16_t height, const uint8_t* image, uint8_t scale) {
	const uint8_t *pointer = image;
	uint16_t scaled_width = width * scale;
	uint16_t scaled_height = height * scale;
	uint16_t i, j;
	uint16_t xlen = 0;
	for (uint16_t yy = y; yy < y + scaled_height; yy += scale) {
		for (uint16_t xx = x; xx < x + scaled_width; xx += scale) {
//			lcd_fill(xx, yy, xx + scale - 1, yy + scale - 1, ((*(pointer + 1)) << 8) | (*pointer));
			xlen = scale;
			for (i = yy; i <= yy + scale - 1; i++)
			{
				// lcd_set_cursor(sx, i);
				lcd_wr_regno(lcddev.setxcmd);
				lcd_wr_data(xx >> 8);
				lcd_wr_data(xx & 0XFF);
				lcd_wr_regno(lcddev.setycmd);
				lcd_wr_data(i >> 8);
				lcd_wr_data(i & 0XFF);
				// lcd_write_ram_prepare();
				LCD_RS(0);
				LCD_CS(0);
				LCD_DATA_OUT(lcddev.wramcmd);
				LCD_WR(0);
				LCD_WR(1);
				LCD_CS(1);
				for (j = 0; j < xlen; j++)
				{
					lcd_wr_data(((*(pointer + 1)) << 8) | (*pointer));     /* д       */
				}
			}
			pointer += 2;
		}
	}
}

void lcd_show_video_frame(uint8_t scale) {
	uint16_t sx = 120 - (WIDTH * scale) / 2;
	uint16_t sy = 100 - (HEIGHT * scale) / 2;
	lcd_show_image(sx, sy, WIDTH, HEIGHT, img, scale);
}

void lcd_show_time() {
	uint16_t x = 40, y = 200;
	lcd_show_char(x + 16, y, ':', 16, 0, BLACK);
	lcd_show_num(x, y, sec_count / 60, 2, 16, BLACK);
	if (sec_count / 60 < 10) {
	  lcd_show_num(x, y, 0, 1, 16, BLACK);
	}
	lcd_show_num(x + 24, y, sec_count % 60, 2, 16, BLACK);
	if (sec_count % 60 < 10) {
	  lcd_show_num(x + 24, y, 0, 1, 16, BLACK);
	}
}

void lcd_show_button() {
	if (sel_button == 0) {
		lcd_show_image(102, 190, 36, 36, pause, 1);
	} else {
		lcd_show_image(102, 190, 36, 36, play, 1);
	}
}

void lcd_show_playback_speed() {
	uint16_t x = 120 + (WIDTH * 3) / 2 - 17, y = 200;
	if (sel_playback_speed == 0) { // x1
		lcd_fill(x - 24, y, x, y + 16, WHITE);
		lcd_show_char(x - 8, y, 'x', 16, 0, BLACK);
		lcd_show_num(x, y, 1, 1, 16, BLACK);
	} else if (sel_playback_speed == 1) { // x2
		lcd_show_char(x - 8, y, 'x', 16, 0, RED);
		lcd_show_num(x, y, 2, 1, 16, RED);
	} else { // x0.5
		lcd_show_char(x - 24, y, 'x', 16, 0, BLUE);
		lcd_show_num(x - 16, y, 0, 1, 16, BLUE);
		lcd_show_char(x - 8, y, '.', 16, 0, BLUE);
		lcd_show_num(x, y, 5, 1, 16, BLUE);
	}
}

uint8_t key_scan() {
	static uint8_t key_up = 1;
	static uint8_t double_pressed = 0;
	uint8_t key_val = 0;

	if (key_up && (KEY0_PRESSED || KEY1_PRESSED || KEY_WK_PRESSED)) {
		HAL_Delay(10);
		if (KEY_WK_PRESSED) {
			key_val = KEY_WK_VAL;
		} else if (KEY0_PRESSED) {
			key_val = KEY0_VAL;
		} else if (KEY1_PRESSED) {
			key_val = KEY1_VAL;
		}
		key_up = 0;
		double_pressed = 0;
	} else if (!KEY0_PRESSED && !KEY1_PRESSED && !KEY_WK_PRESSED) {
		key_up = 1;
		double_pressed = 0;
	} else if (!double_pressed) {
		HAL_Delay(10);
		if (KEY0_PRESSED && KEY_WK_PRESSED) {
			key_val = KEY_WK_0_VAL;
			double_pressed = 1;
		} else if (KEY1_PRESSED && KEY_WK_PRESSED) {
			key_val = KEY_WK_1_VAL;
			double_pressed = 1;
		} else {
			double_pressed = 0;
		}
	}

	return key_val;
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
  lcd_clear(WHITE);
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  MX_TIM3_Init();
  /* USER CODE BEGIN 2 */
  HAL_TIM_Base_Start_IT(&htim3);
  HAL_UART_Receive_IT(&huart1, (uint8_t *)rxBuffer, 1);

  lcd_show_image(0, 0, 240, 320, bg, 1);
  lcd_fill(120 - (WIDTH * 3) / 2 - 5, 100 - (HEIGHT * 3) / 2 - 5, 120 + (WIDTH * 3) / 2 - 1 + 5, 233, WHITE);
  lcd_fill(30, 175, 210, 180, 0xbdf7);
  lcd_show_time();
  lcd_show_button();
  lcd_show_playback_speed();
  lcd_fill(120 - (WIDTH * 3) / 2, 100 - (HEIGHT * 3) / 2, 120 + (WIDTH * 3) / 2 - 1, 100 + (HEIGHT * 3) / 2, BLACK);
  lcd_draw_rectangle(120 - (WIDTH * 3) / 2 - 6, 100 - (HEIGHT * 3) / 2 - 6, 120 + (WIDTH * 3) / 2 - 1 + 6, 234, BLACK);
  lcd_draw_rectangle(120 - (WIDTH * 3) / 2 - 7, 100 - (HEIGHT * 3) / 2 - 7, 120 + (WIDTH * 3) / 2 - 1 + 7, 235, BLACK);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  if (frame_ready) {
		  lcd_show_video_frame(scale);
		  lcd_show_progress_bar();
		  if (capture_frame) {
			  uint8_t* src = &img[0];
			  uint8_t* dst = &img_copy[0];
			  for (uint16_t i = 0; i < IMG_SIZE; i++) {
				  *(dst++) = *(src++);
			  }
			  lcd_show_image(30, 260, WIDTH, HEIGHT, img_copy, 1);
			  lcd_draw_rectangle(30 - 1, 260 - 1, 30 + WIDTH, 260 + HEIGHT + 1, BLACK);
			  lcd_draw_rectangle(30 - 2, 260 - 2, 30 + WIDTH + 1, 260 + HEIGHT + 2, BLACK);
			  capture_frame = 0;
		  }
		  frame_ready = 0;
	  }
	  if (update_time) {
		  if (video_status == VIDEO_PLAYING) {
			  lcd_show_time();
			  update_time = 0;
		  }
//		  lcd_show_num(20, 250, frame_count, 5, 24, BLACK);
//		  lcd_show_num(150, 250, frame_total, 5, 24, BLUE);
	  }
	  if (update_button) {
		  lcd_show_button();
		  update_button = 0;
	  }
	  if (update_playback_speed) {
		  lcd_show_playback_speed();
		  update_playback_speed = 0;
	  }

	  volatile uint8_t key_val = key_scan();
	  switch (key_val) {
		  case KEY_WK_VAL:
			  capture_frame = 1;
			  break;
		  case KEY0_VAL:
			  if (scale == 3) {
				  lcd_fill(120 - (WIDTH * 3) / 2, 100 - (HEIGHT * 3) / 2, 120 + (WIDTH * 3) / 2 - 1, 100 + (HEIGHT * 3) / 2, BLACK);
				  scale = 1;
			  } else {
				  scale++;
			  }
			  lcd_show_video_frame(scale);
			  break;
		  case KEY1_VAL:
			  HAL_UART_Transmit(&huart1, &signal[1], 1, 0xffff);
			  HAL_UART_DeInit(&huart1);
			  lcd_clear(WHITE);
			  lcd_show_image(0, 0, 240, 320, bg, 1);
			  video_status = VIDEO_NOT_STARTED;
			  frame_ready = 0;
			  break;
		  case KEY_WK_0_VAL:
			  if (video_status != VIDEO_NOT_STARTED) {
				  update_button = 1;
				  sel_button = 1 - sel_button;
				  if (video_status == VIDEO_PLAYING) {
					  video_status = VIDEO_PAUSED;
				  } else if (video_status == VIDEO_PAUSED) {
					  video_status = VIDEO_PLAYING;
				  }
				  HAL_UART_Transmit(&huart1, &signal[0], 1, 0xffff);
			  }
			  break;
		  case KEY_WK_1_VAL:
			  update_playback_speed = 1;
			  sel_playback_speed = sel_playback_speed == 2 ? 0 : sel_playback_speed + 1;
			  HAL_UART_Transmit(&huart1, &signal[2], 1, 0xffff);
			  break;
		  default:
			  break;
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
