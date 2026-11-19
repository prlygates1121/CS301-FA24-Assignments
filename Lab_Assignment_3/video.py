from time import sleep

import cv2
import numpy as np
import serial
import time
import struct
import sys
import threading

# Global control flags
is_playing = True
terminate = False
playback_speed = 0

def process_video(video_path, frames_per_second):
    # Create video capture object
    cap = cv2.VideoCapture(video_path)
    if not cap.isOpened():
        print("Error: Could not open video.")
        return None

    fps = cap.get(cv2.CAP_PROP_FPS)
    frame_interval = int(fps / frames_per_second)
    all_frame_data = []
    frame_count = 0

    while True:
        ret, frame = cap.read()
        if not ret:
            break

        if frame_count % frame_interval == 0:
            frame_rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
            resized_frame = cv2.resize(frame_rgb, (60, 45), interpolation=cv2.INTER_AREA)
            height, width = resized_frame.shape[:2]
            frame_16bit = np.zeros((height, width), dtype=np.uint16)

            for y in range(height):
                for x in range(width):
                    r = (np.uint16(resized_frame[y, x, 0]) >> 3) << 11
                    g = ((np.uint16(resized_frame[y, x, 1]) >> 2) & 0x3F) << 5
                    b = (resized_frame[y, x, 2] >> 3) & 0x1F
                    frame_16bit[y, x] = r | g | b

            flat_array = frame_16bit.flatten()
            all_frame_data.append(flat_array)

        frame_count += 1

    cap.release()
    return all_frame_data


def read_serial():
    global is_playing, playback_speed, terminate
    while not terminate:
        try:
            if ser.in_waiting:
                raw_data = ser.read(1)
                command = int.from_bytes(raw_data)
                print(f"Received command: ", command)
                if command == 0:
                    print("Toggle pause/play")
                    is_playing = not is_playing
                elif command == 1:
                    print("Terminate")
                    is_playing = False
                    terminate = True
                    return
                elif command == 2:
                    if playback_speed == 0:
                        playback_speed = 1
                    elif playback_speed == 1:
                        playback_speed = 2
                    elif playback_speed == 2:
                        playback_speed = 0
                    print(f"Playback speed set to {playback_speed}x")

        except serial.SerialException:
                break


# Main execution
if __name__ == "__main__":
    # Process video first
    video_path_main = sys.argv[1] if len(sys.argv) > 1 else "path/to/video.mp4"
    print("Processing video...")
    all_frame_data_main = process_video(video_path_main, frames_per_second=8)
    print("Video processing completed.")

    # Setup serial connection
    ser = serial.Serial("COM5", 1000000, 8, "N", 1, timeout=None)
    ser.dtr = False
    ser.rts = False
    print("Serial port opened.")

    # Start reading thread
    print("Starting read thread...")
    read_thread = threading.Thread(target=read_serial, daemon=True)
    read_thread.start()
    print("Read thread started.")
    time.sleep(0.5)

    try:
        # Send total number of frames
        total_frames = len(all_frame_data_main)
        ser.write(total_frames.to_bytes(2, byteorder='little'))

        # Send frames of a video
        for frame_index, frame_data in enumerate(all_frame_data_main):
            if terminate:
                break
            if playback_speed == 1 and frame_index % 2 == 0 and frame_index != total_frames - 1:
                continue
            while True:
                # Send a frame
                if is_playing:
                    # Send frame header
                    frame_marker = 0x1234
                    ser.write(frame_marker.to_bytes(2, byteorder='little'))
                    # Send frame data
                    for value in frame_data:
                        ser.write(value.astype(np.uint16).tobytes())
                    # Send frame index
                    ser.write(frame_index.to_bytes(2, byteorder='little'))
                    print(f"Frame {frame_index} sent.")
                    if playback_speed == 2:
                        time.sleep(0.135)
                    break
                time.sleep(0.001)
        terminate = True
    except Exception as e:
        print(f"Error occurred: {e}")

    finally:
        # Cleanup
        read_thread.join(timeout=1)
        ser.close()
        print("Program completed.")
