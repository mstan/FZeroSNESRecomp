
pushpc

org $0091E6
  JSL Gravity_Hijack
  NOP							;Delete Leftover from Vanilla Code
  
pullpc

;Check whether current lvl is Rainbow Road
Gravity_Hijack:
  LDA $90     					;Current league
  ASL A       					;*2
  ASL A      					;*4
  ADC $90     					;*5
  ADC $53    					;Current race number
  CMP #$32						;Rainbow Road lvl ID
  BNE Regular_Gravity			;Load regular Gravity if not Rainbow Road
  LDA #$38						;Load high Gravity  
  STA $14						;Store Gravity
  
Regular_Gravity:
  STZ $02						;Vanilla Code (0 out Top Speed Low Byte)
  LDA $0B10, X				    ;Vanilla Code (Load Machine Action of current Racer)
  RTL
