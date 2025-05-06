#!/bin/bash

# Create a 10 frame 1bpp bitmap sequence from any gif:

# Check if an input file is provided
if [ -z "$1" ]; then
  echo "[?] Usage: $0 <input_gif>"
  exit 1
fi

# Input GIF file from arguments
input_gif="$1"

# Extract the file name without the extension
file_name=$(basename "$input_gif" | sed 's/\(.*\)\..*/\1/')

if [ ! -d $file_name ]; then
  echo "[+] Creating $file_name directory..."
  mkdir -p $file_name || exit 1
fi

# Get the total number of frames using ffprobe
total_frames=$(ffprobe -v error -select_streams v:0 -show_entries stream=nb_frames -of default=noprint_wrappers=1:nokey=1 "$input_gif")

# Check if total_frames is successfully retrieved
if [ -z "$total_frames" ]; then
  echo "[x] Failed to get the total frame count from the GIF."
  exit 1
fi

# Calculate the frame interval (total_frames / 10)
frame_interval=$((total_frames / 10))

# Check if the interval is valid (greater than 0)
if [ "$frame_interval" -le 0 ]; then
  echo "[x] Frame interval is too small. The GIF might have fewer than 10 frames."
  exit 1
fi

# Run ffmpeg to select 10 frames, resize, and crop
ffmpeg -i "$input_gif" -pix_fmt monow -loglevel quiet -start_number 0 -vf "select='not(mod(n\,$frame_interval))',scale=42:-1,crop=42:16" -vsync vfr "${file_name}/${file_name}_%d.bmp"

echo "[+] Frames have been extracted and saved in \"./${file_name}/...\":"
ls "${file_name}/"
