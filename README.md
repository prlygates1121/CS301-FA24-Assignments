# CS301-FA24-Assignments

Coursework assignments for **CS301 (Embedded Systems and Microcomputer Principle)**, Fall 2024, implemented on **STM32F103RCT6** using STM32CubeIDE and the STM32 HAL library.

## Directory Structure

```text
.
├── Lab_Assignment_2/   # Hamming Code Encoder & Decoder
├── Lab_Assignment_3/   # UART Video Player (STM32 firmware + video.py)
├── Lab_Assignment_4/   # Touchscreen Side-Scrolling Runner Game
├── .gitignore
└── README.md
```

## Assignments

### Lab Assignment 2 — Hamming Code Encoder & Decoder
- Supports 11 variants from `Hamming(3,1)` to `Hamming(15,11)` (defaults to `Hamming(9,5)`).
- **Controls:**
  - `KEY_WKUP`: Toggle between `ENCODE` and `DECODE` modes
  - `KEY0` / `KEY1`: Input bit `0` / `1` (or clear after completion)
  - `KEY0 + KEY1`: Cycle through Hamming code variants
- Displays parity/data bits on the TFT-LCD and performs single-bit error detection/correction as well as multiple-error detection.

### Lab Assignment 3 — UART Video Player
- Streams `60×45` RGB565 frames from a PC to the STM32 over `USART1` (`1000000` baud) using [`Lab_Assignment_3/video.py`](Lab_Assignment_3/video.py).
- **Controls:**
  - `KEY0`: Cycle display scale (`1x` / `2x` / `3x`)
  - `KEY_WKUP`: Capture current frame as a thumbnail
  - `KEY_WKUP + KEY0`: Pause / resume playback
  - `KEY_WKUP + KEY1`: Cycle playback speed (`x1` / `x2` / `x0.5`)
  - `KEY1`: Stop playback and reset

### Lab Assignment 4 — Touchscreen Runner Game
- Side-scrolling runner game on TFT-LCD with resistive touchscreen input and RLE-compressed sprite rendering.
- **Controls:**
  - On-screen touch buttons: Move up/down across 3 lanes and jump over obstacles
  - Vertical touch swipe: Adjust scrolling speed (`-5` to `+5`)
  - `KEY0` / `KEY1`: Switch between two game modes/themes

## Usage

1. Open or import `Lab_Assignment_2`, `Lab_Assignment_3`, or `Lab_Assignment_4` in **STM32CubeIDE**, then build and flash to an STM32F103RCT6 board.
2. For **Lab Assignment 3**, install the host dependencies and run the streamer script:
   ```bash
   pip install opencv-python numpy pyserial
   ```
   Update the video path and serial port (`COM5` by default) in `Lab_Assignment_3/video.py`, then run:
   ```bash
   python Lab_Assignment_3/video.py
   ```
