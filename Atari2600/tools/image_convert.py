from PIL import Image

WIDTH = 8
HEIGHT = 8

# Load the image and convert to RGBA
img = Image.open("jellyfish.png").convert("RGBA")

# Resize to exactly 8x8 pixels
img = img.resize((WIDTH, HEIGHT), Image.NEAREST)
pixels = img.load()

# Initialize sprite array
GRP0 = [0] * HEIGHT  # 8 rows of 8 pixels

# Loop through each pixel
for y in range(HEIGHT):
    for x in range(WIDTH):
        r, g, b, a = pixels[x, y]
        # Consider any non-transparent pixel as "set" (black or colored)
        if a > 0 and (r < 128 and g < 128 and b < 128):  # dark pixel
            GRP0[y] |= 1 << (7 - x)  # Bit 7 = leftmost

# Print DASM .BYTE array
print("GRP0_DATA:")
for b in GRP0:
    print(f"    .BYTE %{b:08b}")
