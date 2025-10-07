
    PROCESSOR 6502
    INCLUDE "vcs.h"

; Constants
BLUE           = $84
GREY           = $0A
VBLANK_LINES   = 37
VISIBLE_LINES  = 192
OVERSCAN_LINES = 33
MIN_Y          = 10
MAX_Y          = 182
SPRITE_HEIGHT  = 8
START_Y        = 90

; Zero Page
    SEG.U VARS
    ORG $80
SpriteY   ds 1      ; vertical position of player
Scanline  ds 1      ; current scanline in visible area
RowIndex  ds 1      ; index into player arrays

;player player registers
    SEG CODE
    ORG $F000

; player player data/coordinates
GRP0_DATA:
    .BYTE %00000000
    .BYTE %00001100
    .BYTE %11010010
    .BYTE %10100001
    .BYTE %10100001
    .BYTE %11010010
    .BYTE %00001100
    .BYTE %00000000

GRP1_DATA:
    .BYTE %01111110
    .BYTE %10000001
    .BYTE %10000001
    .BYTE %01111110
    .BYTE %01001010
    .BYTE %00101001
    .BYTE %10100100
    .BYTE %01000000

RESET:
    SEI
    CLD
    LDX #$FF
    TXS

    ; Clear RAM
    LDA #0
    TAX
ClrRAM:
    STA $ffeb,X
    INX
    BNE ClrRAM

    ; Init colors
    LDA #BLUE
    STA COLUBK
    LDA #GREY
    STA COLUP0
    STA COLUP1

    ; Init player vertical position
    LDA #START_Y
    STA SpriteY

    ; Horizontal position (fixed)
    LDA #50
    STA RESP0
    STA RESP1

MainLoop:
; VSYNC
    LDA #2
    STA VSYNC
    STA WSYNC
    STA WSYNC
    STA WSYNC
    LDA #0
    STA VSYNC

; VBLANK
    LDA #2
    STA VBLANK

    ; Movement
    LDA INPT4
    BMI MoveUp
    ; Fire not pressed: move down by 1
    LDA SpriteY
    SEC
    SBC #2
    CMP #MAX_Y
    BCC StoreY
    ; If exceeded bottom, reset
    LDA #START_Y
    STA SpriteY
    JMP SkipMove

MoveUp:
    LDA SpriteY
    SEC
    ADC #1
    CMP #MIN_Y
    BCC ResetTop
    
StoreY:
    STA SpriteY
    JMP SkipMove

ResetTop:
    LDA #START_Y
    STA SpriteY

SkipMove:
    LDY #VBLANK_LINES
    
VBLoop:
    STA WSYNC
    DEY
    BNE VBLoop

; Visible Area
    LDA #0
    STA VBLANK
    LDA #0
    STA Scanline

VisibleLoop:
    ; Compute row index = Scanline - SpriteY
    LDA Scanline
    SEC
    SBC SpriteY
    STA RowIndex

    ; Only draw player if RowIndex < SPRITE_HEIGHT
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

; OVERSCAN
    LDA #2
    STA VBLANK
    LDY #OVERSCAN_LINES
    
OSLoop:
    STA WSYNC
    DEY
    BNE OSLoop

    JMP MainLoop

; Vectors
    ORG $FFFA
    .WORD RESET
    .WORD RESET
    .WORD RESET
