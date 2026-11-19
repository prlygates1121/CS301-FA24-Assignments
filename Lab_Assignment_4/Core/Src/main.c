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
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "lcd_v4.h"
#include "images.h"
#include "touch.h"
#include "24cxx.h"
#include <limits.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define LINE1 50
#define LINE2 100
#define LINE3 150
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
uint8_t mode = 0;
uint8_t mode_transition = 0;

uint16_t canvas_back[60][320];

int16_t canvas_speed = 120;
int16_t speed = 5;

int16_t x_main = 100;
int16_t y_main = 115;
int16_t x_main_last = 100;
int16_t x_main_dst = 100;
int16_t distance = 0;

int16_t x_bad_guy = 0;
int16_t y_bad_guy = 0;

uint16_t touch_index = 0;
uint16_t touch_index_last = 0;
uint16_t touch_list[50][2];
uint8_t scroll_active = 1;
uint8_t scroll_first = 1;
int8_t scroll_trend = 0;

uint8_t draw_usagi_now = 0;
uint8_t draw_canvas_now = 0;
uint8_t draw_finish_line_now = 0;
uint8_t update_timer = 0;

uint8_t up_pressed = 0;
uint8_t down_pressed = 0;
uint8_t jump_pressed = 0;

uint8_t going_up = 0;
uint8_t going_down = 0;
uint8_t jumping = 0;

uint8_t has_bad_guy = 0;

uint16_t sec_count = 0;

uint8_t hit = 0;

uint8_t end = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void HAL_Delay_us(uint32_t us) {
	__HAL_TIM_SET_COUNTER(&htim4, 0);
	while (__HAL_TIM_GET_COUNTER(&htim4) < us);
}

void lcd_draw_point_ex(uint16_t x, uint16_t y, uint32_t color) {
	if (color == BLACK) {
		if (x > 58) {
			lcd_fast_draw_point(x, y, g_back_color);
		}
	} else {
		lcd_fast_draw_point(x, y, color);
	}
}

void lcd_show_time() {
	uint16_t x = 132, y = 300;
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

void lcd_show_image(int16_t x, int16_t y, const uint8_t* image, uint8_t flip) {
	const uint8_t *pointer = image;
	int16_t width = (*(pointer + 3) << 8) | (*(pointer + 2));
	int16_t height = (*(pointer + 5) << 8) | (*(pointer + 4));
	pointer += 8;
	uint32_t color;
	if (flip == 1) {
		// [x, x + height)
		for (int16_t xx = x; xx < x + height; xx++) {
			// [y, y + width - 1]
			for (int16_t yy = y; yy < y + width; yy++) {
				if (yy >= 0 && yy < lcddev.height) {
					color = ((*(pointer + 1)) << 8) | (*pointer);
					lcd_draw_point_ex(xx, yy, color);
				}
				pointer += 2;
			}
		}
	} else {
		// [x, x + height)
		for (int16_t xx = x; xx < x + height; xx++) {
			// [y, y + width - 1]
			for (int16_t yy = y + width - 1; yy >= y; yy--) {
				if (yy >= 0 && yy < lcddev.height) {
					color = ((*(pointer + 1)) << 8) | (*pointer);
					lcd_draw_point_ex(xx, yy, color);
				}
				pointer += 2;
			}
		}
	}
}

void lcd_show_image_RLE(int16_t x, int16_t y, const uint8_t* image, uint8_t flip, uint8_t wrap_around) {
	const uint8_t *pointer = image;
	int16_t width = (*(pointer + 2) << 8) | *(pointer + 3);
	int16_t height = (*(pointer + 4) << 8) | *(pointer + 5);
	uint16_t color = 0;
	uint8_t count = 0;
	uint8_t repeating = 1;

	pointer += 6;

	int16_t x_start = x;
	int16_t x_end = x + height;
	int16_t x_step = 1;

	int16_t y_start = flip == 1 ? y : (y + width - 1);
	int16_t y_end = flip == 1? y + width : y;
	int16_t y_step = flip == 1? 1 : -1;

	uint8_t in_screen;

	for (int16_t xx = x_start; xx < x_end; xx += x_step) {
		set_cursor_x(xx);
		for (int16_t yy = y_start; (flip == 1? yy < y_end : yy >= y_end); yy += y_step) {
			in_screen = yy >= 0 && yy < lcddev.height;

			// new count value incoming
			if (count == 0) {
				// non-repeating sequence
				if ((*pointer | 0x7F) == 0xFF) {
					count = (*pointer & 0x7F);
					pointer++;		// move pointer to the next color value
					repeating = 0;
				}
				// repeating sequence
				else {
					count = *pointer;
					color = ((*(pointer + 1)) << 8) | *(pointer + 2);
					pointer += 3;	// move pointer to the start of next count value
					repeating = 1;
				}
			}

			if (repeating == 1) {
				if (in_screen) {
					set_cursor_y(yy);
					lcd_ram();
					if (color == BLACK) {
						if (xx > 58) {
							lcd_wr_data(g_back_color);

						} else {
							lcd_wr_data(canvas_back[xx][yy]);
						}
					} else {
						lcd_wr_data(color);
					}
				} else if (wrap_around) {
					set_cursor_y(((yy + lcddev.height) % lcddev.height));
					lcd_ram();
					if (color == BLACK) {
						if (xx > 58) {
							lcd_wr_data(g_back_color);

						} else {
							lcd_wr_data(canvas_back[xx][((yy + lcddev.height) % lcddev.height)]);
						}
					} else {
						lcd_wr_data(color);
					}
				}
				count--;
			} else {
				if (in_screen) {
					color = ((*pointer) << 8) | *(pointer + 1);
					set_cursor_y(yy);
					lcd_ram();
					if (color == BLACK) {
						if (xx > 58) {
							lcd_wr_data(g_back_color);
						} else {
							lcd_wr_data(canvas_back[xx][yy]);
						}
					} else {
						lcd_wr_data(color);
					}
				} else if (wrap_around) {
					color = ((*pointer) << 8) | *(pointer + 1);
					set_cursor_y(((yy + lcddev.height) % lcddev.height));
					lcd_ram();
					if (color == BLACK) {
						if (xx > 58) {
							lcd_wr_data(g_back_color);
						} else {
							lcd_wr_data(canvas_back[xx][((yy + lcddev.height) % lcddev.height)]);
						}
					} else {
						lcd_wr_data(color);
					}
				}
				pointer += 2;
				count--;
			}


		}
	}
}


void lcd_fill_ex(int16_t sx, int16_t sy, int16_t ex, int16_t ey, uint32_t color, uint8_t hide) {
    for (int16_t x = sx; x < ex; x++) {
        set_cursor_x(x % lcddev.width);
        for (int16_t y = sy; y < ey; y++) {
            int16_t wrapped_y = (y + lcddev.height) % lcddev.height;
			if (hide == 0 || (x_main != LINE2 || wrapped_y < y_main || wrapped_y > y_main + 60)) {
				set_cursor_y(wrapped_y);
				lcd_ram();
				lcd_wr_data(color);
			}
        }
    }
}


void draw_canvas() {
	static int16_t offset = 0;
	
	const uint8_t *pointer = mode == 0 ? canvas : sky;
	int16_t width = 320;
	int16_t height = 60;
	uint16_t color = 0;
	uint8_t count = 0;
	uint8_t repeating = 1;

	pointer += 6;

	uint8_t in_screen;

	for (int16_t xx = 0; xx < height; xx++) {
		set_cursor_x(xx);
		for (int16_t yy = offset + width - 1; yy >= offset; yy--) {
			in_screen = yy >= 0 && yy < lcddev.height;

			// new count value incoming
			if (count == 0) {
				// non-repeating sequence
				if ((*pointer | 0x7F) == 0xFF) {
					count = (*pointer & 0x7F);
					pointer++;		// move pointer to the next color value
					repeating = 0;
				}
				// repeating sequence
				else {
					count = *pointer;
					color = ((*(pointer + 1)) << 8) | *(pointer + 2);
					pointer += 3;	// move pointer to the start of next count value
					repeating = 1;
				}
			}

			uint16_t y_print = in_screen ? yy : ((yy + lcddev.height) % lcddev.height);
			if (repeating == 1) {
				if (canvas_back[xx][y_print] != color) {
					set_cursor_y(y_print);
					lcd_ram();
					lcd_wr_data(color);
					canvas_back[xx][y_print] = color;
				}
				count--;
			} else {
				color = ((*pointer) << 8) | *(pointer + 1);
				if (canvas_back[xx][y_print] != color) {
					set_cursor_y(y_print);
					lcd_ram();
					lcd_wr_data(color);
					canvas_back[xx][y_print] = color;
				}
				pointer += 2;
				count--;
			}
		}
	}

	if (mode == 0) {
		// use lcd_fill in a for loop to draw the black blocks
		static int16_t offset_track = 0;
		offset_track += 3 * speed;
		if (offset_track <= -320) {
			offset_track += 320;
		} else if (offset_track >= 320) {
			offset_track -= 320;
		}
		for (int16_t i = -20; i < 300; i += 20) {
			int16_t y_start = offset_track + i;
			int16_t y_end = offset_track + i + 20;
			uint32_t block_color = i % 40 == 0 ? BROWN : g_back_color;
			// draw the block by for loop
			lcd_fill_ex(125, y_start, 127, y_end, block_color, 1);
			lcd_fill_ex(175, y_start, 177, y_end, block_color, 0);
		}
	}



	offset += 3 * speed;
	if (offset <= -320) {
		offset += 320;
	} else if (offset >= 320) {
		offset -= 320;
	}
}


void draw_main() {
	uint8_t flip = speed > 0 ? 0 : 1;
	if (mode == 0) {
		if (jumping == 0) {
			if (speed >= 4 || speed <= -4) {
				static uint8_t run_state = 1;
				switch(run_state) {
					case 1:
						lcd_show_image_RLE(x_main, y_main, usagi_run_1, flip, 1);
						break;
					case 2:
						lcd_show_image_RLE(x_main, y_main, usagi_run_2, flip, 1);
						break;
					case 3:
						lcd_show_image_RLE(x_main, y_main, usagi_run_3, flip, 1);
						break;
					case 4:
						lcd_show_image_RLE(x_main, y_main, usagi_run_2, flip, 1);
						break;
				}
				run_state = run_state == 4 ? 1 : (run_state + 1);
			} else {
				static uint8_t walk_state = 1;
				switch(walk_state) {
					case 1:
						lcd_show_image_RLE(x_main, y_main, usagi_walk_1, flip, 1);
						break;
					case 2:
						lcd_show_image_RLE(x_main, y_main, usagi_walk_2, flip, 1);
						break;
					case 3:
						lcd_show_image_RLE(x_main, y_main, usagi_walk_3, flip, 1);
						break;
					case 4:
						lcd_show_image_RLE(x_main, y_main, usagi_walk_2, flip, 1);
						break;
				}
				walk_state = walk_state == 4 ? 1 : (walk_state + 1);
			}
		} else {
			lcd_show_image_RLE(x_main, y_main, usagi_jump, flip, 1);
		}
	} else {
		if (hit == 0) {
			static uint8_t fly_state = 1;
			if (fly_state == 1) {
				// clear the previous image
				lcd_fill(x_main - 13, y_main, x_main + 3, y_main + 65, g_back_color);
				lcd_show_image_RLE(x_main + 3, y_main, hachi_flying, flip, 1);
			} else if (fly_state == 2) {
				// clear the previous image
				lcd_fill(x_main + 67, y_main, x_main + 83, y_main + 65, g_back_color);
				lcd_show_image_RLE(x_main - 3, y_main, hachi_flying, flip, 1);
			}
			fly_state = fly_state == 1 ? 2 : 1;
		} else {
			lcd_show_image_RLE(x_main, y_main, hachi_hit, flip, 1);
		}

	}

	if (x_main_dst < x_main || (x_main_dst == x_main && x_main_last > x_main)) {
		// Clear Usagi's feet when going up
		lcd_fill(x_main + 70, y_main, x_main + 120, y_main + 65, g_back_color);
	} else if (x_main_dst > x_main || (x_main_dst == x_main && x_main_last < x_main)) {
		// Clear Usagi's ears when going down
		lcd_fill(((x_main - 20 < 58) ? 58 : (x_main - 20)), y_main, (x_main < 58 ? 58 : x_main), y_main + 60, g_back_color);
		if (x_main <= 50) {
			int16_t clear_air_x_start = x_main - 20 < 0 ? 0 : (x_main - 20);
			int16_t clear_air_x_end = x_main;
			int16_t clear_air_y_start = speed >= 0 ? (y_main + 28) : y_main;
			int16_t clear_air_y_end = speed >= 0 ? (y_main + 60) : (y_main + 30);
			for (uint16_t x = clear_air_x_start; x < clear_air_x_end; x++) {
				set_cursor_x(x);
				for (uint16_t y = clear_air_y_start; y < clear_air_y_end; y++) {
					if (x < 58) {
						set_cursor_y(y);
						lcd_ram();
						lcd_wr_data(canvas_back[x][y]);
					} else {
						set_cursor_y(y);
						lcd_ram();
						lcd_wr_data(g_back_color);
					}
				}
			}
		}
	}
	x_main_last = x_main;
}


void clear_bad_guy() {
	int16_t clear_y_start = (y_bad_guy - 20 < 0) ? 0 : (y_bad_guy - 20);
	int16_t clear_y_end = (y_bad_guy + 86 >= 320) ? 320 : (y_bad_guy + 86);
	for (int16_t x = x_bad_guy; x < x_bad_guy + 70; x++) {
		set_cursor_x(x);
		for (int16_t y = clear_y_start; y < clear_y_end; y++) {
			if (x < 58) {
				set_cursor_y(y);
				lcd_ram();
				lcd_wr_data(canvas_back[x][y]);
			} else {
				set_cursor_y(y);
				lcd_ram();
				lcd_wr_data(g_back_color);
			}
		}
	}
}

void draw_bad_guy() {
	if (y_bad_guy >= 321 || y_bad_guy < -64) {
		has_bad_guy = 0;
		if (y_bad_guy >= 321) {
			// fill the left side of the screen with black
			lcd_fill(x_bad_guy, 305, x_bad_guy + 60, 320, g_back_color);
		}
		y_bad_guy = 0;
		clear_bad_guy();
	} else {
		if (mode == 0) {
			lcd_show_image_RLE(x_bad_guy, y_bad_guy, witch, 0, 0);
		} else {
			lcd_show_image_RLE(x_bad_guy, y_bad_guy, spider, 0, 0);
		}
		if (speed > 0) {
			int16_t clear_y_start = (y_bad_guy - 15 < 0) ? 0 : (y_bad_guy - 15);
			for (int16_t x = x_bad_guy; x < x_bad_guy + 70; x++) {
				set_cursor_x(x);
				for (int16_t y = clear_y_start; y < y_bad_guy; y++) {
					if (x < 58) {
						set_cursor_y(y);
						lcd_ram();
						lcd_wr_data(canvas_back[x][y]);
					} else {
						set_cursor_y(y);
						lcd_ram();
						lcd_wr_data(g_back_color);
					}
				}
			}
		} else if (speed < 0) {
			int16_t clear_y_start;
			int16_t clear_y_end;
			if (mode == 1) {
				clear_y_start = (y_bad_guy + 45 >= 320) ? 320 : (y_bad_guy + 45);
				clear_y_end = (y_bad_guy + 86 >= 320) ? 320 : (y_bad_guy + 86);
			} else {
				clear_y_start = (y_bad_guy + 65 >= 320) ? 320 : (y_bad_guy + 65);
				clear_y_end = (y_bad_guy + 86 >= 320) ? 320 : (y_bad_guy + 86);
			}

			for (int16_t x = x_bad_guy; x < x_bad_guy + 70; x++) {
				set_cursor_x(x);
				for (int16_t y = clear_y_start; y < clear_y_end; y++) {
					if (x < 58) {
						set_cursor_y(y);
						lcd_ram();
						lcd_wr_data(canvas_back[x][y]);
					} else {
						set_cursor_y(y);
						lcd_ram();
						lcd_wr_data(g_back_color);
					}
				}
			}
		}
		y_bad_guy += 3 * speed;
	}
}

void draw_finish_line() {
	static int16_t offset = 0;
	static int16_t offset_last = 0;
	if (offset >= 100) {
		draw_finish_line_now = 0;
		offset = 0;
		end = 1;
		return;
	}
	if (mode == 0) {
		lcd_fill(60, offset, 240, offset + 4, RED);
	} else {
		lcd_fill(60, offset, 240, offset + 4, WHITE);
	}
	if (offset != 0) {
		lcd_fill(60, offset_last, 240, offset_last + 4, g_back_color);
	}
	offset_last = offset;
	offset += 3 * speed;
}

void clear_main() {
	// remove Usagi from the screen
	int16_t clear_x_start = (x_main - 20 < 0) ? 0 : (x_main - 20);
	int16_t clear_x_end = x_main + 120;
	int16_t clear_y_start = (y_main - 15 < 0) ? 0 : (y_main - 15);
	int16_t clear_y_end = y_main + 65 > 320 ? 320 : (y_main + 65);
	for (int16_t x = clear_x_start; x < clear_x_end; x++) {
		set_cursor_x(x);
		for (int16_t y = clear_y_start; y < clear_y_end; y++) {
			if (x < 58) {
				set_cursor_y(y);
				lcd_ram();
				lcd_wr_data(canvas_back[x][y]);
			} else {
				set_cursor_y(y);
				lcd_ram();
				lcd_wr_data(g_back_color);
			}
		}
	}
}

uint8_t hit_obstacle() {
    if (x_main + 50 < x_bad_guy + 30 || x_bad_guy + 50 < x_main + 30) {
    	return 0;
    }
    if (mode == 0) {
        if (y_main + 60 < y_bad_guy || y_bad_guy + 60 < y_main) {
        	return 0;
        }
	} else {
		if (y_main + 55 < y_bad_guy || y_bad_guy + 45 < y_main) {
			return 0;
		}
	}

    return 1;
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
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM4_Init();
  MX_TIM3_Init();
  MX_TIM2_Init();
  MX_TIM5_Init();
  /* USER CODE BEGIN 2 */
  // TIM4 is used to generate delay in us
  HAL_TIM_Base_Start(&htim4);
  // TIM3 is a timer for inertia
  HAL_TIM_Base_Start_IT(&htim3);

  HAL_TIM_Base_Start_IT(&htim2);

  HAL_TIM_Base_Start_IT(&htim5);

  lcd_init();
  tp_dev.init();
  g_back_color = 0xf7bb;
  lcd_clear(g_back_color);

  mode = 0;

//  update_canvas();
//  draw_canvas();

  uint16_t y_button_base = 310;
  lcd_draw_rectangle(220, y_button_base - 50, 235, y_button_base, BLACK);
  lcd_draw_rectangle(220, y_button_base - 110, 235, y_button_base - 60, BLUE);
  lcd_draw_rectangle(220, y_button_base - 300, 235, y_button_base - 250, RED);
  lcd_draw_line(223, y_button_base - 25, 232, y_button_base - 10, BLACK);
  lcd_draw_line(232, y_button_base - 40, 223, y_button_base - 25, BLACK);
  lcd_draw_line(232, y_button_base - 85, 223, y_button_base - 70, BLUE);
  lcd_draw_line(223, y_button_base - 100, 232, y_button_base - 85, BLUE);
  uint16_t x_touch;
  uint16_t y_touch;

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  static uint8_t key0_pressed = 0;
	  static uint8_t key1_pressed = 0;
		if (HAL_GPIO_ReadPin(KEY0_GPIO_Port, KEY0_Pin) == GPIO_PIN_RESET && key0_pressed == 0) {
			key0_pressed = 1;
			mode_transition = 1;
			mode = 0;
		} else if (HAL_GPIO_ReadPin(KEY1_GPIO_Port, KEY1_Pin) == GPIO_PIN_RESET && key1_pressed == 0) {
			key1_pressed = 1;
			mode_transition = 1;
			mode = 1;
		} else {
			mode_transition = 0;
			key0_pressed = 0;
			key1_pressed = 0;
		}
		if (mode_transition == 1) {
			if (mode == 0) {
				g_back_color = 0xf7bb;
			} else {
				g_back_color = 0xb71e;
			}
			has_bad_guy = 0;
			sec_count = 0;
			distance = 0;
			speed = 5;
			x_main = 100;
			y_main = 115;
			x_main_last = 100;
			x_main_dst = 100;
			x_bad_guy = 0;
			lcd_show_time();
			lcd_clear(g_back_color);
			// reset canvas_back
			for (int16_t i = 0; i < 60; i++) {
				for (int16_t j = 0; j < 320; j++) {
					canvas_back[i][j] = g_back_color;
				}
			}
			lcd_draw_rectangle(220, y_button_base - 50, 235, y_button_base, BLACK);
			lcd_draw_rectangle(220, y_button_base - 110, 235, y_button_base - 60, BLUE);
			lcd_draw_rectangle(220, y_button_base - 300, 235, y_button_base - 250, RED);
			  lcd_draw_line(223, y_button_base - 25, 232, y_button_base - 10, BLACK);
			  lcd_draw_line(232, y_button_base - 40, 223, y_button_base - 25, BLACK);
			  lcd_draw_line(232, y_button_base - 85, 223, y_button_base - 70, BLUE);
			  lcd_draw_line(223, y_button_base - 100, 232, y_button_base - 85, BLUE);
		}
		if (end == 1) {
			lcd_clear(g_back_color);
			lcd_show_string(0, 0, 240, 16, 16, "Game Over", BLACK);
			lcd_show_string(0, 20, 240, 16, 16, "Time (in seconds):", BLACK);
			lcd_show_num(140, 20, sec_count, 5, 16, BLACK);
			if (sec_count < 25) {
				lcd_show_string(0, 40, 240, 16, 16, "Very Fast!", BLACK);
			} else if (sec_count < 30) {
				lcd_show_string(0, 40, 240, 16, 16, "Not Bad!", BLACK);
			} else {
				lcd_show_string(0, 40, 240, 16, 16, "Too Slow", BLACK);
			}
			HAL_Delay(5000);
			end = 0;
			sec_count = 0;
			distance = 0;
			speed = 5;
			x_main = 100;
			y_main = 115;
			x_main_last = 100;
			x_main_dst = 100;
			y_bad_guy = 0;
			touch_index = 0;
			touch_index_last = 0;
			scroll_active = 1;
			scroll_first = 1;
			scroll_trend = 0;
			draw_usagi_now = 0;
			draw_canvas_now = 0;
			draw_finish_line_now = 0;
			update_timer = 0;
			up_pressed = 0;
			down_pressed = 0;
			jump_pressed = 0;
			going_up = 0;
			going_down = 0;
			jumping = 0;
			has_bad_guy = 0;
			hit = 0;
			// reset canvas_back
			for (int16_t i = 0; i < 60; i++) {
				for (int16_t j = 0; j < 320; j++) {
					canvas_back[i][j] = g_back_color;
				}
			}
			clear_main();
			clear_bad_guy();
			lcd_clear(g_back_color);
			lcd_draw_rectangle(220, y_button_base - 50, 235, y_button_base, BLACK);
			lcd_draw_rectangle(220, y_button_base - 110, 235, y_button_base - 60, BLUE);
			lcd_draw_rectangle(220, y_button_base - 300, 235, y_button_base - 250, RED);
			  lcd_draw_line(223, y_button_base - 25, 232, y_button_base - 10, BLACK);
			  lcd_draw_line(232, y_button_base - 40, 223, y_button_base - 25, BLACK);
			  lcd_draw_line(232, y_button_base - 85, 223, y_button_base - 70, BLUE);
			  lcd_draw_line(223, y_button_base - 100, 232, y_button_base - 85, BLUE);
		}
	  if (hit == 1) {
		  lcd_fill(x_main + 20, y_main + 60, x_main + 70, y_main + 90, g_back_color);
		  draw_main();
		  HAL_Delay(200);
		  clear_main();
		  HAL_Delay(200);
		  draw_main();
		  HAL_Delay(200);
		  clear_main();
		  HAL_Delay(200);
		  draw_main();
		  hit = 0;
	  }

	  if (draw_usagi_now == 1) {
		  draw_canvas();

		  distance += speed;
		  if (has_bad_guy == 0) {
			  x_bad_guy = (HAL_GetTick() % 3 + 1) * 50;
			  if (speed > 0) {
					y_bad_guy = -64;
				} else {
					y_bad_guy = 320;
			  }
			  has_bad_guy = 1;
		  }
		  if (x_main > x_bad_guy) {
			  if (has_bad_guy == 1) {
				  draw_bad_guy();
				  if (hit_obstacle() == 1 && jumping == 0) {
					  hit = 1;
					  speed = 0;
					  clear_bad_guy();
					  has_bad_guy = 0;
				  }
			  }
			  draw_main();
		  } else {
				draw_main();
				if (has_bad_guy == 1) {
					draw_bad_guy();
					if (hit_obstacle() == 1 && jumping == 0) {
						hit = 1;
						speed = 0;
						clear_bad_guy();
						has_bad_guy = 0;
					}
				}
		  }
			if (distance >= 600 && draw_finish_line_now == 0) {
				draw_finish_line_now = 1;
			}
			if (draw_finish_line_now == 1) {
				draw_finish_line();
			}
		  draw_usagi_now = 0;
	  }

	 if (update_timer == 1) {
	 	update_timer = 0;
	 	lcd_show_time();
	 }


	  // scan the touch
	  tp_dev.scan(0);
	  // if the screen is touched
	  if (tp_dev.sta & TP_PRES_DOWN) {
		  x_touch = tp_dev.x[0];
		  y_touch = tp_dev.y[0];
		  // if the touch coordinate is valid
		  if (x_touch < lcddev.width && y_touch < lcddev.height) {
			  // If this touch is different from the last one
			  if (touch_list[touch_index_last][0] != x_touch || touch_list[touch_index_last][1] != y_touch) {
//				  lcd_draw_point(x_touch, y_touch, RED);
				  // Record it in touch_list
				  touch_list[touch_index][0] = x_touch;
				  touch_list[touch_index][1] = y_touch;

				  // Scrolling
				  if (scroll_first == 1) {
					  scroll_first = 0;
				  } else if (scroll_active == 1) {


					  // If this touch is on the right of last touch
					  if (touch_list[touch_index_last][1] - y_touch > 1) {
						  if (scroll_trend < 0) {
							  scroll_trend = 0;
						  } else {
							  scroll_trend++;
							  if (scroll_trend == 6 - (speed > 0 ? speed : -speed)) {
								  HAL_GPIO_TogglePin(LED0_GPIO_Port, LED0_Pin);
								  __HAL_TIM_SET_COUNTER(&htim3, 0);
								  if (speed < 5) {
									  speed++;
								  }
								  scroll_active = 0;
								  scroll_first = 1;
								  scroll_trend = 0;
							  }
						  }

					  } else if (y_touch - touch_list[touch_index_last][1] > 1) {
						  if (scroll_trend > 0) {
							  scroll_trend = 0;
						  } else {
							  scroll_trend--;
							  if (scroll_trend == -6 + (speed > 0 ? speed : -speed)) {
								  __HAL_TIM_SET_COUNTER(&htim3, 0);
								  if (speed > -5) {
									  speed--;
								  }
								  scroll_active = 0;
								  scroll_first = 1;
								  scroll_trend = 0;
							  }
						  }

					  }
				  }

				  // Moving up
				  if (x_touch >= 215 && x_touch <= 235 && y_touch >= y_button_base - 50 && y_touch <= y_button_base) {
					  if (up_pressed == 0) {
						  lcd_fill(220, y_button_base - 50, 235, y_button_base, BLACK);
						  up_pressed = 1;
					  }
					  if (speed != 0 && (x_main == LINE2 || x_main == LINE3)) {
						  x_main_dst = x_main - 50;
						  going_up = 1;
					  } else if (x_main == LINE1) {
						  speed = 0;
						  hit = 1;
					  }
				  }

				  // Moving down
				  if (x_touch >= 215 && x_touch <= 235 && y_touch >= y_button_base - 110 && y_touch <= y_button_base - 60) {
					  if (down_pressed == 0) {
						  lcd_fill(220, y_button_base - 110, 235, y_button_base - 60, BLUE);
						  down_pressed = 1;
					  }
					  if (speed != 0 && (x_main == LINE1 || x_main == LINE2)) {
						  x_main_dst = x_main + 50;
						  going_down = 1;
						} else if (x_main == LINE3) {
							speed = 0;
							hit = 1;
						}
				  }

				  // Jump
				  if (x_touch >= 215 && x_touch <= 235 && y_touch >= y_button_base - 300 && y_touch <= y_button_base - 250) {
					  if (jump_pressed == 0) {
						  lcd_fill(220, y_button_base - 300, 235, y_button_base - 250, RED);
						  jump_pressed = 1;
					  }
					  if (speed != 0 && jumping == 0 && (x_main == LINE1 || x_main == LINE2 || x_main == LINE3)) {
						  x_main_dst = x_main - 50;
						  jumping = 1;
					  }
				  }

				  touch_index_last = touch_index;
				  touch_index = (touch_index == 49) ? 0 : (touch_index + 1);
			  }
		  }
	  } else { // if the screen is not touched
		  scroll_active = 1;
		  scroll_first = 1;
		  scroll_trend = 0;
		  if (up_pressed == 1) {
			  lcd_fill(220, y_button_base - 50, 235, y_button_base, g_back_color);
			  lcd_draw_rectangle(220, y_button_base - 50, 235, y_button_base, BLACK);
			  // draw an upper arrow by drawing 2 lines
			  lcd_draw_line(223, y_button_base - 25, 232, y_button_base - 10, BLACK);
			  lcd_draw_line(232, y_button_base - 40, 223, y_button_base - 25, BLACK);
			  up_pressed = 0;
		  }
		  if (down_pressed == 1) {
			  lcd_fill(220, y_button_base - 110, 235, y_button_base - 60, g_back_color);
			  lcd_draw_rectangle(220, y_button_base - 110, 235, y_button_base - 60, BLUE);
			  // draw a down arrow by drawing 2 lines
			  lcd_draw_line(232, y_button_base - 85, 223, y_button_base - 70, BLUE);
			  lcd_draw_line(223, y_button_base - 100, 232, y_button_base - 85, BLUE);
			  down_pressed = 0;
		  }
		  if (jump_pressed == 1) {
			  lcd_fill(220, y_button_base - 300, 235, y_button_base - 250, g_back_color);
			  lcd_draw_rectangle(220, y_button_base - 300, 235, y_button_base - 250, RED);
			  jump_pressed = 0;
		  }
	  }

	  int16_t speed_abs;
	  if (speed < 0) {
		  speed_abs = -speed;
		  lcd_show_char(190, 300, '-', 16, 0, BLUE);
	  } else {
		  lcd_show_char(190, 300, ' ', 16, 0, BLUE);
		  speed_abs = speed;
	  }
	  lcd_show_num(200, 300, speed_abs, 1, 16, BLUE);
//	  lcd_show_num(170, 300, jumping, 1, 16, RED);
//	  lcd_show_num(120, 0, x_main, 3, 16, BLUE);
//	  lcd_show_num(150, 0, y_main, 3, 16, BLUE);
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
