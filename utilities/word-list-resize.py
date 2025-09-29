import random
import sys
import os


def select_words(input_file, target_size=20 * 1024):
    output_file = f"{input_file.split('.')[0]}_resized_{int(target_size)}.txt"
    # Read all words from the input file
    with open(input_file, "r", encoding="utf-8") as f:
        words = f.read().splitlines()

    # Filter words that are >= 4 characters
    words = [w for w in words if len(w) >= 4]

    if not words:
        raise ValueError("No words of length >= 4 found in the input file.")

    selected = []
    current_size = 0

    # Keep adding random words until reaching target size
    while current_size < target_size:
        word = random.choice(words)
        selected.append(word)
        current_size = sum(len(w) + 1 for w in selected)  # +1 for '\n'

    selected = sorted(selected)

    # Write to output file
    with open(output_file, "w", encoding="utf-8") as f:
        f.write("\n".join(selected))

    print(
        f"Output file '{output_file}' created with size {os.path.getsize(output_file)} bytes."
    )


if __name__ == "__main__":
    if len(sys.argv) < 2:
        raise ValueError("Error: No arguments - specify file size in KB")
    if len(sys.argv) == 3:
        select_words(sys.argv[1], float(sys.argv[2]) * 1024)
    else:
        select_words(sys.argv[1])
