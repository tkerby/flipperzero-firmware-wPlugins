from PIL import Image

WIDTH = 16
HEIGHT = 8

# Load the image and convert to RGBA
img = Image.open("fish.png").convert("RGBA")

# Resize to exactly 16x8 pixels (Atari 2600 sprite size)
img = img.resize((WIDTH, HEIGHT), Image.NEAREST)
pixels = img.load()

# Initialize sprite arrays
GRP0 = [0] * 8  # Left 8 pixels
GRP1 = [0] * 8  # Right 8 pixels

# Loop through each pixel
for y in range(HEIGHT):      # 8 rows
    for x in range(WIDTH): # 16 columns
        r, g, b, a = pixels[x, y]
        if a > 0 and r == 0 and g == 0 and b == 0:  # black pixel
            if x < 8:
                GRP0[y] |= 1 << (7 - x)           # Bit 7 = leftmost
            else:
                GRP1[y] |= 1 << (15 - x)          # Bit 15-x -> bit 7-x for GRP1

# Print DASM .BYTE arrays
print("GRP0_DATA:")
for b in GRP0:
    print(f"    .BYTE %{b:08b}")

print("\nGRP1_DATA:")
for b in GRP1:
    print(f"    .BYTE %{b:08b}")
