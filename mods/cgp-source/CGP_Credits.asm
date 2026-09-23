
pushpc

;Do not show Credits on any League after beating Expert Mode
;Instead, show Credits after beating the last League on any Difficulty Level

org $039B9F                        
  LDA $90							;Load Current League
  CMP #$0A							;Check whether it was the last League
  
;Change Credits Sequence
;First Line sets the Starting X Pos
;Line after that is Text ($4A = A, $63 = Z)
;$00 starts a new line, next Byte is Y distance for new line, next Byte after that X distance
;Then comes a Text Line again etc.

org $02C74F

;Project Lead

db $50

db $59, $5B, $58, $53, $4E, $4C, $5D,$FF, $55, $4E, $4A, $4D
db $00, $20, $5C

db $60, $58, $5B, $5D, $51, $62,$FF, $56, $4F
db $00, $10, $44

db $4F, $4E, $57, $57, $58, $5B, $FF, $5F, $52, $5B, $4A, $5C, $5D, $4A, $5B
db $00, $60, $3C

;Tracks and Design

db $5D, $5B, $4A, $4C, $54, $5C, $FF, $4A, $57, $4D, $FF, $4D, $4E, $5C, $52, $50, $57
db $00, $20, $5C

db $60, $58, $5B, $5D, $51, $62, $FF, $56, $4F
db $00, $10, $44

db $4F, $4E, $57, $57, $58, $5B, $FF, $5F, $52, $5B, $4A, $5C, $5D, $4A, $5B
db $00, $10, $54

db $4F, $5B, $52, $4D, $4A, $62, $60, $52, $5D, $4C, $51
db $00, $10, $58

db $63, $4E, $59, $51, $62, $5B, $5E, $56, $42, $45
db $00, $10, $58

db $56, $58, $5E, $5C, $51, $52, $54, $58, $56, $52
db $00, $10, $68

db $4E, $5B, $52, $54, $46, $44
db $00, $10, $68

db $5F, $5E, $55, $4D, $5E, $5F
db $00, $60, $44

;Sound and Music

db $5C, $58, $5E, $57, $4D, $FF, $4A, $57, $4D, $FF, $56, $5E, $5C, $52, $4C
db $00, $20, $3C

db $5C, $52, $55, $5F, $4E, $5B, $5B, $4E, $59, $55, $58, $52, $4D, $FF, $4C, $55, $4E
db $00, $10, $50

db $5D, $51, $4E, $4B, $55, $5E, $5B, $4C, $4A, $4F, $4E
db $00, $10, $50

db $4C, $58, $4B, $4A, $55, $5D, $FF, $5C, $5D, $4A, $5B
db $00, $10, $50

db $4C, $58, $5C, $56, $52, $4C, $5D, $4A, $52, $55, $63
db $00, $10, $60

db $4F, $4A, $4C, $4E, $4C, $4A, $5D
db $00, $10, $4C

db $4C, $58, $4D, $62, $FF, $58, $6A, $5A, $5E, $52, $57, $57
db $00, $10, $50

db $4D, $58, $54, $5D, $58, $5B, $FF, $4F, $52, $55, $55
db $00, $60, $64

;Coding

db $4C, $58, $4D, $52, $57, $50
db $00, $20, $44

db $4F, $4E, $57, $57, $58, $5B, $FF, $5F, $52, $5B, $4A, $5C, $5D, $4A, $5B
db $00, $60, $60

;Testing

db $5D, $4E, $5C, $5D, $52, $57, $50
db $00, $20, $58

db $4A, $50, $4E, $57, $5D, $62, $58, $5E, $5C
db $00, $10, $58

db $4B, $4A, $5E, $59, $4E, $57, $57, $4E, $5B
db $00, $10, $50

db $4C, $4A, $5C, $5E, $4A, $55, $5D, $58, $56, $48, $42
db $00, $10, $4C

db $56, $54, $4D, $5C, $56, $4A, $5C, $5D, $4E, $5B, $49, $41
db $00, $10, $5C

db $56, $4A, $5D, $58, $55, $4A, $4B, $5E
db $00, $60, $44

;Special Thanks

db $5C, $59, $4E, $4C, $52, $4A, $55, $FF, $5D, $51, $4A, $57, $54, $5C
db $00, $20, $60

db $4C, $4A, $5D, $4A, $4D, $58, $5B
db $00, $10, $60

db $50, $5B, $4E, $50, $58, $42, $4D
db $00, $10, $44

db $5B, $5E, $52, $63, $FF, $4A, $57, $4D, $FF, $5D, $52, $5D, $58, $58
db $00, $10, $1C

db $4E, $5C, $4C, $58, $4E, $61, $4E, $FF, $4A, $57, $4D, $FF, $51, $4A, $57, $5C, $4C, $58, $5C, $5D, $5E, $4D, $52, $58
db $00, $10, $48

db $4F, $5B, $4A, $4C, $5D, $4A, $55, $FF, $57, $58, $56, $4A, $4D
db $00, $80, $54

;Thanks for Playing

db $60, $4E, $FF, $51, $58, $59, $4E, $FF, $62, $58, $5E
db $00, $10, $3C

db $4E, $57, $53, $58, $62, $4E, $4D, $FF, $5D, $51, $4E, $FF, $50, $4A, $56, $4E, $6C
db $00, $20, $4C

db $5C, $4E, $4E, $FF, $62, $58, $5E, $FF, $4A, $50, $4A, $52, $57, $64
db $00, $70, $00

pullpc
