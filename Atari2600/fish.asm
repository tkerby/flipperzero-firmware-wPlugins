;***************************************************************
; Atari 2600: Move a grey pixel up when the fire button is pressed
;***************************************************************

    PROCESSOR 6502
    INCLUDE "vcs.h"

;---------------------------------------------------------------
; Constants (NTSC)
;---------------------------------------------------------------
BLUE       = $84   ; Background color
GREY       = $0A   ; Player color
VBLANK_LINES  = 37
VISIBLE_LINES = 192
OVERSCAN_LINES= 33
MIN_Y = 10
MAX_Y = 180

;---------------------------------------------------------------
; Zero page variables
;---------------------------------------------------------------
    SEG.U VARS
    ORG $80
PlayerY   ds 1      ; Vertical position of the pixel
ButtonState ds 1

;---------------------------------------------------------------
; Code section
;---------------------------------------------------------------
    SEG CODE
    ORG $F000

RESET:
    SEI
    CLD
    LDX #$FF
    TXS

    ; Clear RAM
    LDA #0
    TAX
ClearRAM:
    STA $00,X
    INX
    BNE ClearRAM

    ; Init display colors
    LDA #BLUE
    STA COLUBK
    LDA #GREY
    STA COLUP0

    ; Init position
    LDA #100
    STA PlayerY

    ; Place player horizontally (roughly middle)
    LDA #0
    STA RESP0

MainLoop:
;-----------------------
; VSYNC (3 lines)
;-----------------------
    LDA #2
    STA VSYNC
    STA WSYNC
    STA WSYNC
    STA WSYNC
    LDA #0
    STA VSYNC

;-----------------------
; VBLANK (37 lines)
;-----------------------
    LDA #2
    STA VBLANK

    ; --- Input ---
    LDA INPT4       ; Fire button (bit 7 = 0 when pressed)
    BMI NoPress
    ; Button pressed → move up
    LDA PlayerY
    CMP #MIN_Y
    BEQ NoPress
    SEC
    SBC #1
    STA PlayerY
NoPress:

    ; Wait out the rest of VBLANK
    LDY #VBLANK_LINES
VBLoop:
    STA WSYNC
    DEY
    BNE VBLoop

;-----------------------
; Visible area (192 lines)
;-----------------------
    LDA #0
    STA VBLANK      ; Turn on beam

    LDY #0
VisibleLoop:
    STA WSYNC

    CPY PlayerY
    BNE SkipPixel

    ; Draw pixel for 1 scanline
    LDA #%10000000
    STA GRP0
    JMP AfterPixel

SkipPixel:
    LDA #0
    STA GRP0

AfterPixel:
    INY
    CPY #VISIBLE_LINES
    BNE VisibleLoop

;-----------------------
; Overscan (33 lines)
;-----------------------
    LDA #2
    STA VBLANK
    LDY #OVERSCAN_LINES
OSLoop:
    STA WSYNC
    DEY
    BNE OSLoop

    JMP MainLoop

;---------------------------------------------------------------
; Vectors
;---------------------------------------------------------------
    ORG $FFFA
    .WORD RESET
    .WORD RESET
    .WORD RESET
