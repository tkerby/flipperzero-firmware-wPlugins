from PIL import Image

img = Image.open("fish.png").convert("RGBA")
pixels = img.load()

width, height = img.size

# Initialize sprite arrays
sprite_height = height
GRP0 = [0] * sprite_height  # left 8 pixels
GRP1 = [0] * sprite_height  # right 8 pixels

# Loop through all pixels
for y in range(height):
    for x in range(width):
        r, g, b, a = pixels[x, y]
        if a > 0 and r == 0 and g == 0 and b == 0:  # black pixel
            row = y
            col = x
            if 0 <= col < 8:
                GRP0[row] |= 1 << (7 - col)      # bit 7 = leftmost
            elif 8 <= col < 16:
                GRP1[row] |= 1 << (15 - col)     # bit 15-x → bit 7-x for GRP1

# Print DASM arrays
print("GRP0_DATA:")
for b in GRP0:
    print(f"    .BYTE %{b:08b}")

print("\nGRP1_DATA:")
for b in GRP1:
    print(f"    .BYTE %{b:08b}")
