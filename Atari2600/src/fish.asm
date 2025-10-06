;***************************************************************
; Atari 2600: Fish sprite with Flappy-Bird style movement
; Solid blue background
; Fish moves down automatically, moves up when fire pressed
; Resets if it touches top or bottom
;***************************************************************

    PROCESSOR 6502
    INCLUDE "vcs.h"

;---------------------------------------------------------------
; Constants
;---------------------------------------------------------------
BLUE           = $84
GREY           = $0A
VBLANK_LINES   = 37
VISIBLE_LINES  = 192
OVERSCAN_LINES = 33
MIN_Y          = 10
MAX_Y          = 180
SPRITE_HEIGHT  = 8
START_Y        = 90
START_X        = 00

;---------------------------------------------------------------
; Zero page
;---------------------------------------------------------------
    SEG.U VARS
    ORG $80
PlayerY   ds 1      ; vertical position of sprite
Scanline  ds 1      ; current scanline in visible area
RowIndex  ds 1      ; index into sprite arrays

;---------------------------------------------------------------
; Fish sprite data (16x8)
; Replace these with your Pillow-generated GRP0/GRP1
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
    LDA #START_Y
    STA PlayerY

    ; Horizontal position (fixed)
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

    ; --- Flappy movement ---
    LDA INPT4
    BMI MoveUp
    ; Fire not pressed: move down by 1
    LDA PlayerY
    CLC
    SBC #1
    CMP #MAX_Y
    BCC StoreY
    ; If exceeded bottom, reset
    LDA #START_Y
    STA PlayerY
    JMP SkipMove

MoveUp:
    LDA PlayerY
    SEC
    ADC #2
    CMP #MIN_Y
    BCC ResetTop
StoreY:
    STA PlayerY
    JMP SkipMove

ResetTop:
    LDA #START_Y
    STA PlayerY

SkipMove:

    LDY #VBLANK_LINES
VBLoop:
    STA WSYNC
    DEY
    BNE VBLoop

;-----------------------
; Visible area (192 lines)
;-----------------------
    LDA #0
    STA VBLANK
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
