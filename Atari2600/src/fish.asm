    PROCESSOR 6502
    INCLUDE "vcs.h"

; Constants
BLUE           = $9E
ORANGE         = $2A
PINK           = $5C
VBLANK_LINES   = 37
VISIBLE_LINES  = 192
OVERSCAN_LINES = 33
MIN_Y          = 10
MAX_Y          = 182
SPRITE_HEIGHT  = 8
START_Y        = 80
JELLY_Y        = 8

; these are *delay counts*, not pixel values
JELLY_RIGHT_DELAY = 80     ; start far right
JELLY_LEFT_DELAY  = 10     ; stop near left edge


; Zero Page Variables
    SEG.U VARS
    ORG $80

JellyDelay   ds 1     ; delay counter controlling X position
SpriteY      ds 1
Scanline     ds 1
RowIndex     ds 1
FrameCounter ds 1


; Sprite Graphics
    SEG CODE
    ORG $F000

GRP0_DATA:
    .BYTE %00000000
    .BYTE %00001100
    .BYTE %11011110
    .BYTE %11111111
    .BYTE %11111111
    .BYTE %11011110
    .BYTE %00001100
    .BYTE %00000000

GRP1_DATA:
    .BYTE %01111110
    .BYTE %11111111
    .BYTE %11111111
    .BYTE %01111110
    .BYTE %01001010
    .BYTE %00101001
    .BYTE %10100100
    .BYTE %01000000


; RESET

RESET:
    SEI
    CLD
    LDX #$FF
    TXS

    LDA #0
    TAX
ClrRAM:
    STA $80,X
    INX
    BNE ClrRAM

    ; Init colors
    LDA #BLUE   ; background
    STA COLUBK
    LDA #ORANGE ; player
    STA COLUP0
    LDA #PINK   ; jellyfish
    STA COLUP1

    ; Init positions
    LDA #START_Y
    STA SpriteY
    LDA #JELLY_RIGHT_DELAY
    STA JellyDelay


; MAIN LOOP

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

; Move jellyfish delay leftward each frame
    INC FrameCounter
    LDA FrameCounter
    AND #%1111        ; refresh rate
    BNE SkipJelly
    DEC JellyDelay
    LDA JellyDelay
    CMP #JELLY_LEFT_DELAY
    BCS SkipReset
    LDA #JELLY_RIGHT_DELAY
    STA JellyDelay
    
SkipReset:
SkipJelly:

; Move player vertically
    LDA INPT4
    BMI MoveUp
    LDA SpriteY
    SEC
    SBC #2
    CMP #MAX_Y
    BCC StoreY
    LDA #START_Y
    STA SpriteY
    JMP SkipMove
    
MoveUp:
    LDA SpriteY
    CLC
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

; Wait out remaining VBLANK time
    LDY #VBLANK_LINES
    
VBLoop:
    STA WSYNC
    DEY
    BNE VBLoop


; Position Jellyfish (Player 1)
    STA WSYNC
    STA HMCLR
    LDY JellyDelay
    
DelayLoop:
    DEY
    BNE DelayLoop        ; wait variable number of cycles
    STA RESP1            ; strobe coarse X
    STA WSYNC
    STA HMOVE


; Visible Area
    LDA #0
    STA VBLANK
    LDA #0
    STA Scanline

VisibleLoop:
    ; Player 0
    LDA Scanline
    SEC
    SBC SpriteY
    STA RowIndex
    LDA RowIndex
    CMP #SPRITE_HEIGHT
    BCC DrawP0
    LDA #0
    STA GRP0
    JMP SkipP0
DrawP0:
    TAX
    LDA GRP0_DATA,X
    STA GRP0
SkipP0:

    ; Jellyfish (P1)
    LDA Scanline
    SEC
    SBC #JELLY_Y
    STA RowIndex
    LDA RowIndex
    CMP #SPRITE_HEIGHT
    BCC DrawP1
    LDA #0
    STA GRP1
    JMP SkipP1
    
DrawP1:
    TAX
    LDA GRP1_DATA,X
    STA GRP1
    
SkipP1:
    STA WSYNC
    INC Scanline
    LDA Scanline
    CMP #VISIBLE_LINES
    BNE VisibleLoop

; Overscan
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
