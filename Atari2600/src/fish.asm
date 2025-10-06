;***************************************************************
; Atari 2600: Solid blue + 16x8 fish sprite
; Sprite moves up with joystick fire button
;***************************************************************

    PROCESSOR 6502
    INCLUDE "vcs.h"

;---------------------------------------------------------------
; NTSC timing
;---------------------------------------------------------------
BLUE           = $84
GREY           = $0A
VBLANK_LINES   = 37
VISIBLE_LINES  = 192
OVERSCAN_LINES = 33
MIN_Y          = 10
MAX_Y          = 180
SPRITE_HEIGHT  = 8

;---------------------------------------------------------------
; Zero page
;---------------------------------------------------------------
    SEG.U VARS
    ORG $80
PlayerY   ds 1      ; vertical position of sprite
Scanline  ds 1      ; current scanline in visible area
RowIndex  ds 1      ; index into sprite arrays

;---------------------------------------------------------------
; Sprite data (16x8 fish)
; Left 8 pixels -> GRP0, Right 8 pixels -> GRP1
;---------------------------------------------------------------
    SEG CODE
    ORG $F000

GRP0_DATA:
    .BYTE %00000000
    .BYTE %11000011
    .BYTE %10101100
    .BYTE %10010000
    .BYTE %10010000
    .BYTE %10101100
    .BYTE %11000011
    .BYTE %00000000

GRP1_DATA:
    .BYTE %11000000
    .BYTE %00110000
    .BYTE %00001100
    .BYTE %00000010
    .BYTE %00000010
    .BYTE %00001100
    .BYTE %00110000
    .BYTE %11000000

;---------------------------------------------------------------
RESET:
    SEI
    CLD
    LDX #$FF
    TXS

    ; Clear RAM
    LDA #0
    TAX
ClrRAM:
    STA $00,X
    INX
    BNE ClrRAM

    ; Init colors
    LDA #BLUE
    STA COLUBK
    LDA #GREY
    STA COLUP0
    STA COLUP1

    ; Init sprite vertical position
    LDA #50
    STA PlayerY

    ; Horizontal position
    LDA #50
    STA RESP0
    STA RESP1

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
    LDA INPT4
    BMI NoPress
    LDA PlayerY
    CMP #MIN_Y
    BEQ NoPress
    SEC
    SBC #1
    STA PlayerY
NoPress:

    LDY #VBLANK_LINES
VBLoop:
    STA WSYNC
    DEY
    BNE VBLoop

;-----------------------
; Visible area (192 lines)
;-----------------------
    LDA #0
    STA VBLANK      ; beam ON
    LDA #0
    STA Scanline

VisibleLoop:
    ; Compute row index = Scanline - PlayerY
    LDA Scanline
    SEC
    SBC PlayerY
    STA RowIndex

    ; Only draw sprite if RowIndex < SPRITE_HEIGHT
    LDA RowIndex
    CMP #SPRITE_HEIGHT
    BCC DrawSprite
SkipSprite:
    LDA #0
    STA GRP0
    STA GRP1
    JMP AfterSprite
DrawSprite:
    LDA RowIndex
    TAX
    LDA GRP0_DATA,X
    STA GRP0
    LDA GRP1_DATA,X
    STA GRP1
AfterSprite:
    STA WSYNC
    INC Scanline
    LDA Scanline
    CMP #VISIBLE_LINES
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
