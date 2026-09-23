
pushpc

org $0BEC94                        ;Moves the s-jet icon to a new location
db $FF,$EF,$62,$35,$FF,$EF,$62,$35,$F5,$14,$5F,$35
  
org $00803E
  JML sub_routine_zero

org $008791
  JSR $AA97
  RTL
  
org $00B82F           
        JSL new_boost_duration        ;Jump to boost duration Routine
        NOP                                                ;remove overlap with overwritten code
        NOP  

pullpc

;Table for how much Energy should be trained. (Value will be doubled in the end)
;Byte 1: Blue Falcon
;Byte 2; Wild Goose
;Byte 3; Golden Fox
;Byte 4; Fire Stingray
boost_drain:
    dw 0022, 0017, 0022, 0021        ;table for health drain while boosting. 1: WC, 2: WG, 3: RG, 4:FS

duration_table:
    db 150, 150, 150, 150                ;table for s-jet duration. 1: WC, 2: WG, 3: GF, 4:FS

sub_routine_zero:
  JSR sub_routine_one
  LDA $60
  BEQ a_
  JML $00804B
a_:
  LDA $54
  JML $008044

sub_routine_one:
  PHA
  PHX
  PHY
  LDA $0054
  CMP #$02
  BNE b_
  LDA $0055
  CMP #$03
  BNE b_
  JSR sub_routine_two
  JSR sub_routine_three
  JSR Drain_Routine  
b_:
  PLY
  PLX
  PLA
  RTS

sub_routine_two:
  LDA $0B10
  ORA #$40
  STA $0B10
  RTS

sub_routine_three:
  LDA $0F53
  BEQ c_
  LDA #$08
  BIT $0D51
  BNE c_
  REP #$20
  LDA $00C9
  BMI d_
  BEQ d_
  SEP #$20
  BRA e_
;disable s-jets when no health left
d_:
  SEP #$20
  STZ $0CF3
  LDA #$35
  STA $02B3
  STA $02B7
  STA $02BB
  BRA c_
e_:
;enable s-jets when enough health
  LDA $0CF3
  SBC #$03
  BEQ c_
  LDA $0CF8
  BNE c_
  LDA #$01
  STA $0CF8
c_:
  RTS

Drain_Routine:
; only drain health if you are using an s-jet AND having no dash applied                               
  LDA $0D51                                        ;load machine state
  AND #$38                                     ;check for s-jet boost, dash plate start, dash plate main
  CMP #$08                                     ;don't branch if ONLY the s-jet is active, otherwise branch
  BNE End_Drain_Routine  
 ; stop boost if race is finished  
  LDA #$05                                                                                ;#$5 is the lap counter after race is finished
  CMP $0D40                                                                                ;lap counter
  BEQ End_Drain_Routine
; stop drain if carried by UFO
  LDA $0EE6                                                ;This address is FF when carried by UFO
  BMI End_Drain_Routine  
   
;only drain health ever 8th frame to make the effect not too strong
  LDA $0CF4                               ; Load the boost timer
  AND #$07                                ; Mask bits 0, 1, and 2
  BNE End_Drain_Routine           ; If ANY of those bits are set, branch        

  REP #$20 
  PHX                                            ;save register
  LDX $D1                                        ;load current player car in register
  LDA $00C9                                        ;load player health
  SBC.l boost_drain,X                ;look at the newly defined table for energy drain amount
  STA $00C9                               ;store to player health          
  BMI overboosted                          ;branch if health reached 0
  BRA h_
  
overboosted:                                          
  STZ $00C9                                        ;make sure you have at least 0 health after overboosting
  LDA #$0001
  STA $0CF4                                        ;if overboosted, set boost timer to 1 (0 would make it cycle back to FF before end) to end boost        
h_:
  PLX                                                ;restore register
  SEP #$20                                        ;back to 8 bit math
  JSL $008791
End_Drain_Routine:
  RTS
  
new_boost_duration:        
        PHX                                                ;save register
        LDX $52                                        ;load current player car in register
        LDA.l duration_table,X        ;load new boost duration
    STA $0CF4                       ;store to boost timer        
        PLX                                                ;restore register
    DEC $0CF3                                ;replenish overwritten code (reduce current s-jet count)
        RTL                                         ;Return to regular routine        
  