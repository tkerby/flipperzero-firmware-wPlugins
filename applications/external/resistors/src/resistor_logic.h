#ifndef RESISTOR_LOGIC_HEADERS
#define RESISTOR_LOGIC_HEADERS

typedef enum {
    R3 = 3,
    R4 = 4,
    R5 = 5,
    R6 = 6
} ResistorType;

typedef enum {
    BandBlack = 0,
    BandBrown = 1,
    BandRed = 2,
    BandOrange = 3,
    BandYellow = 4,
    BandGreen = 5,
    BandBlue = 6,
    BandPurple = 7,
    BandGray = 8,
    BandWhite = 9,
    BandGold = 10,
    BandSilver = 11,
    BandPink = 12
} BandColour;

extern const uint8_t CHARS_NUMERIC;
extern const uint8_t CHARS_MULTIPLIER;
extern const uint8_t CHARS_TOLERANCE;
extern const uint8_t CHARS_TEMP_COEFF;
extern const uint8_t CHARS_CALCULATION;

extern double resistor_multiplier;

BandColour alter_resistor_band(
    ResistorType rtype,
    uint8_t band,
    BandColour current_colour,
    int8_t direction);

char* get_colour_short_description(BandColour colour);

bool has_tolerance(ResistorType rtype);
bool has_temp_coeff(ResistorType rtype);

void update_resistance_calculation(ResistorType rtype, BandColour bands[], char string[]);
void update_resistance_tolerance(ResistorType rtype, BandColour colours[], char string[]);
void update_resistance_temp_coeff(ResistorType rtype, BandColour colours[], char string[]);

#endif
