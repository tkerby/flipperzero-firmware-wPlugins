from PIL import Image

# Define the pattern, which looks like an QRCode 
qr_pattern = [
    [1, 1, 1, 0, 1, 0, 0, 1, 1, 1],
    [1, 0, 1, 0, 0, 1, 0, 1, 0, 1],
    [1, 1, 1, 0, 1, 0, 0, 1, 1, 1],
    [0, 0, 0, 0, 0, 1, 0, 0, 0, 0],
    [1, 0, 1, 0, 1, 0, 1, 0, 1, 0],
    [0, 1, 0, 1, 0, 1, 0, 1, 0, 1],
    [0, 0, 0, 0, 1, 0, 1, 0, 0, 0],
    [1, 1, 1, 0, 0, 1, 0, 1, 0, 1],
    [1, 0, 1, 0, 1, 0, 1, 0, 1, 0],
    [1, 1, 1, 0, 0, 1, 0, 0, 0, 1]
]

# Create a new image with mode '1' (1-bit pixels, black and white)
img = Image.new('1', (10, 10))

# Load the pixel data
pixels = img.load()

# Set the pixels based on the pattern
for y in range(10):
    for x in range(10):
        pixels[x, y] = qr_pattern[y][x]

# Save the image
img.save("qrcode_generator.png")

