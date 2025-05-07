class BoardType:
    """Enumeration for the different board types."""

    BOARD_TYPE_PICO_CALC = 0
    BOARD_TYPE_VGM = 1
    BOARD_TYPE_JBLANKED = 2


class PicoType:
    """Enumeration for the different Pico types."""

    PICO_TYPE_PICO = 0  # Raspberry Pi Pico
    PICO_TYPE_PICO_W = 1  # Raspberry Pi Pico W
    PICO_TYPE_PICO_2 = 2  # Raspberry Pi Pico 2
    PICO_TYPE_PICO_2_W = 3  # Raspberry Pi Pico 2 W


class LibraryType:
    """Enumeration for the different library types."""

    LIBRARY_TYPE_PICO_DVI = 0
    LIBRARY_TYPE_TFT = 1


class BoardPins:
    """Set of pins for the board"""

    def __init__(self, sck: int, mosi: int, miso: int, cs: int, dc: int, rst: int):
        self.sck = sck
        self.mosi = mosi
        self.miso = miso
        self.cs = cs
        self.dc = dc
        self.rst = rst


class Board:
    """Class to represent the board."""

    def __init__(
        self,
        board_type,
        pico_type,
        library_type,
        pins,
        width: int,
        height: int,
        rotation: int,
        name: str,
        has_wifi: bool,
    ):
        self.board_type = board_type
        self.pico_type = pico_type
        self.library_type = library_type
        self.pins = pins
        self.width = width
        self.height = height
        self.rotation = rotation
        self.name = name
        self.has_wifi = has_wifi


# Define the board types
VGMConfig: Board = Board(
    board_type=BoardType.BOARD_TYPE_VGM,
    pico_type=PicoType.PICO_TYPE_PICO,
    library_type=LibraryType.LIBRARY_TYPE_PICO_DVI,
    pins=BoardPins(sck=8, mosi=11, miso=12, cs=13, dc=14, rst=15),
    width=320,
    height=240,
    rotation=0,
    name="Video Game Module",
    has_wifi=False,
)

PicoCalcConfigPico: Board = Board(
    board_type=BoardType.BOARD_TYPE_PICO_CALC,
    pico_type=PicoType.PICO_TYPE_PICO,
    library_type=LibraryType.LIBRARY_TYPE_TFT,
    pins=BoardPins(sck=10, mosi=11, miso=12, cs=13, dc=14, rst=15),
    width=320,
    height=320,
    rotation=0,
    name="PicoCalc - Pico",
    has_wifi=False,
)

PicoCalcConfigPicoW: Board = Board(
    board_type=BoardType.BOARD_TYPE_PICO_CALC,
    pico_type=PicoType.PICO_TYPE_PICO_W,
    library_type=LibraryType.LIBRARY_TYPE_TFT,
    pins=BoardPins(sck=10, mosi=11, miso=12, cs=13, dc=14, rst=15),
    width=320,
    height=320,
    rotation=0,
    name="PicoCalc - Pico W",
    has_wifi=True,
)

PicoCalcConfigPico2: Board = Board(
    board_type=BoardType.BOARD_TYPE_PICO_CALC,
    pico_type=PicoType.PICO_TYPE_PICO_2,
    library_type=LibraryType.LIBRARY_TYPE_TFT,
    pins=BoardPins(sck=10, mosi=11, miso=12, cs=13, dc=14, rst=15),
    width=320,
    height=320,
    rotation=0,
    name="PicoCalc - Pico 2",
    has_wifi=False,
)

PicoCalcConfigPico2W: Board = Board(
    board_type=BoardType.BOARD_TYPE_PICO_CALC,
    pico_type=PicoType.PICO_TYPE_PICO_2_W,
    library_type=LibraryType.LIBRARY_TYPE_TFT,
    pins=BoardPins(sck=10, mosi=11, miso=12, cs=13, dc=14, rst=15),
    width=320,
    height=320,
    rotation=0,
    name="PicoCalc - Pico 2 W",
    has_wifi=True,
)

JBlankedPicoConfig: Board = Board(
    board_type=BoardType.BOARD_TYPE_JBLANKED,
    pico_type=PicoType.PICO_TYPE_PICO_W,
    library_type=LibraryType.LIBRARY_TYPE_TFT,
    pins=BoardPins(sck=6, mosi=7, miso=4, cs=5, dc=11, rst=10),
    width=320,
    height=240,
    rotation=3,
    name="JBlanked Pico",
    has_wifi=True,
)
