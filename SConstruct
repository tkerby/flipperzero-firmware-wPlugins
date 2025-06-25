# SConstruct file for Pokemon Yellow+ Flipper Zero app
# This file is used by some Flipper Zero build configurations

Import("env")

# Add source files
sources = [
    "pokemon_main_enhanced.c",
    "pokemon_enhanced.c", 
    "pokemon_integration.c",
    "enhanced_battle_screen.c",
    "pokemon_yellow_data.c",
    "moves_data_manual.c",  # Use manual moves data
    # Include original files as fallback
    "pokemon.c",
    "battle.c", 
    "menu.c",
    "select_screen.c",
    "pokemon_battle.c"
]

# Filter sources to only include files that exist
import os
existing_sources = []
for source in sources:
    if os.path.exists(source):
        existing_sources.append(source)
    else:
        print(f"Warning: Source file {source} not found, skipping")

# Build the library
env.Append(CPPPATH=["."])
lib = env.StaticLibrary("pokemon_yellow_plus", existing_sources)

Return("lib")
