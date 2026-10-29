"""Hand-picked held items for the boss teams. Crystal's gym leaders, Elite
Four, Lance, the rival and Red hold nothing; these give each a fitting item.
One item per mon, in party order; duplicates are fine (Gen 2 has no item
clause). gen_trainers.py checks every boss mon has one and that it does
something when held.

Key: (trainer class, party number in parties.asm, 1-based).
"""

BOSS_ITEMS = {
    ("FALKNER", 1): ["BERRY", "SHARP_BEAK"],
    ("BUGSY", 1): ["BRIGHTPOWDER", "POISON_BARB", "SILVERPOWDER"],
    ("WHITNEY", 1): ["LEFTOVERS", "PINK_BOW"],
    ("MORTY", 1): ["SPELL_TAG", "LEFTOVERS", "SPELL_TAG", "BRIGHTPOWDER"],
    ("CHUCK", 1): ["SCOPE_LENS", "BLACKBELT_I"],
    ("JASMINE", 1): ["MAGNET", "LEFTOVERS", "METAL_COAT"],
    ("PRYCE", 1): ["NEVERMELTICE", "MINT_BERRY", "NEVERMELTICE"],
    ("CLAIR", 1): ["DRAGON_SCALE", "LEFTOVERS", "QUICK_CLAW", "MYSTIC_WATER"],
    ("BROCK", 1): [
        "HARD_STONE",
        "SOFT_SAND",
        "MYSTIC_WATER",
        "LEFTOVERS",
        "SCOPE_LENS",
    ],
    ("MISTY", 1): ["TWISTEDSPOON", "LEFTOVERS", "MIRACLEBERRY", "MYSTIC_WATER"],
    ("LT_SURGE", 1): ["MAGNET", "BRIGHTPOWDER", "MAGNET", "QUICK_CLAW", "LEFTOVERS"],
    ("ERIKA", 1): ["LEFTOVERS", "BRIGHTPOWDER", "SCOPE_LENS", "MIRACLE_SEED"],
    ("JANINE", 1): [
        "SHARP_BEAK",
        "POISON_BARB",
        "LEFTOVERS",
        "BRIGHTPOWDER",
        "TWISTEDSPOON",
    ],
    ("SABRINA", 1): ["TWISTEDSPOON", "LEFTOVERS", "FOCUS_BAND"],
    ("BLAINE", 1): ["LEFTOVERS", "CHARCOAL", "QUICK_CLAW"],
    ("BLUE", 1): [
        "SHARP_BEAK",
        "TWISTEDSPOON",
        "SOFT_SAND",
        "MYSTIC_WATER",
        "MIRACLE_SEED",
        "CHARCOAL",
    ],
    ("WILL", 1): [
        "TWISTEDSPOON",
        "NEVERMELTICE",
        "BRIGHTPOWDER",
        "LEFTOVERS",
        "QUICK_CLAW",
    ],
    ("KOGA", 1): [
        "BRIGHTPOWDER",
        "TWISTEDSPOON",
        "FOCUS_BAND",
        "LEFTOVERS",
        "SHARP_BEAK",
    ],
    ("BRUNO", 1): [
        "SOFT_SAND",
        "BLACKBELT_I",
        "FOCUS_BAND",
        "HARD_STONE",
        "SCOPE_LENS",
    ],
    ("KAREN", 1): [
        "LEFTOVERS",
        "MIRACLE_SEED",
        "SPELL_TAG",
        "BLACKGLASSES",
        "CHARCOAL",
    ],
    ("CHAMPION", 1): [
        "MYSTIC_WATER",
        "MAGNET",
        "NEVERMELTICE",
        "HARD_STONE",
        "CHARCOAL",
        "DRAGON_SCALE",
    ],
    ("RIVAL2", 4): [
        "BLACKGLASSES",
        "SHARP_BEAK",
        "MAGNET",
        "SPELL_TAG",
        "TWISTEDSPOON",
        "LEFTOVERS",
    ],
    ("RIVAL2", 5): [
        "BLACKGLASSES",
        "SHARP_BEAK",
        "MAGNET",
        "SPELL_TAG",
        "TWISTEDSPOON",
        "CHARCOAL",
    ],
    ("RIVAL2", 6): [
        "BLACKGLASSES",
        "SHARP_BEAK",
        "MAGNET",
        "SPELL_TAG",
        "TWISTEDSPOON",
        "MYSTIC_WATER",
    ],
    ("RED", 1): [
        "LIGHT_BALL",
        "TWISTEDSPOON",
        "LEFTOVERS",
        "MIRACLE_SEED",
        "CHARCOAL",
        "MYSTIC_WATER",
    ],
}

# Items the battle engine checks by item id rather than by held effect
# (engine/battle/effect_commands.asm SpeciesItemBoost, core.asm Berserk Gene).
SPECIES_ITEMS = ("LIGHT_BALL", "THICK_CLUB", "METAL_POWDER", "BERSERK_GENE")
