;Makes DMagnets not damage the player and CPU
;Just a Bunch of checks for DMagnet Tiles. DMag Flag is #$08, so I reduced all the original values by #$08

pushpc

org $0098B1

  AND #$04			;Exclude DMag Flag from Check
  
org $0098BC

  AND #$F4			;Exclude DMag Flag from Check
  
org $0098CE  

  AND #$14			;Exclude DMag Flag from Check
  
org $0098F1  

  AND #$F4			;Exclude DMag Flag from Check
    
org $0098F9  

  BIT #$04			;Exclude DMag Flag from Check
  
org $009292
  JML Grip_Hijack
  NOP
  
org $009B59
  JML Hover_Altitude_Hijack
  
org $00B890
  JSL Turning_Hijack
  rep 26 : NOP					;Delete Leftovers from Vanilla Code
  
org $00B8E6
  JSL Strafing_Hijack
  rep 9 : NOP					;Delete Leftovers from Vanilla Code
    
pullpc

;Gives the Player High Grip if they are on DMagnet Tiles while on Ground
;Check for DMag and Ground first
Grip_Hijack:
  LDA $0D51						;Check Machine State
  BMI Regular_Grip_Routine		;Branch if Midair
  LDA $0D50						;Load Surface Flags
  BIT #$08						;DMag BIT
  BEQ Regular_Grip_Routine		;Branch if not on a DMag
;Player is on a DMag and on Ground, give them high Grip
  LDA #$02						;Load Hi Byte for Grip Gain
  STA $18						;Store to Scratchpad RAM for Grip Gain Hi Byte
  LDA #$20						;Load Lo Byte for Grip Gain
  STA $17						;Store to Scratchpad RAM for Grip Gain Lo Byte
  LDA #$80						;Load Minor Sound Effect
  TSB $0AD3						;Store to Sound Effect Flags 3
  JML $0092F9					;Return

;Make the Car not bounce up and down while you are on a DMag to appear more planted to the ground 
;Check for DMag first 
Hover_Altitude_Hijack:
  LDX #$00						;Load Player Index
  LDA $0D50						;Load Surface Flags
  BIT #$08						;DMag BIT
  BNE No_Acceleration_Hover		;Branch if on a DMag 
  LDA $E1						;Load Blast Pipe Effect Init
  BEQ No_Acceleration_Hover		;Branch if not Accelerating
  JML $009B5F					;If you are Accelerating and not on DMag, go to Hover Altitude Change Calculation
  No_Acceleration_Hover:
  JML $009B85					;If not Accelerating or on DMAG, Skip Hover Bounce Calc 
;Player is not on A DMag on Ground 
Regular_Grip_Routine:
  LDA $1D						;Check Slide Amount
  CMP $FA81, Y					;Compare to Slide Amount Threshold Table
  BCS Check_Instant_Grip		;Branch if sliding more
  JML $009297					;Jump Back to where it normally would be if sliding less than the Threshold
  
Check_Instant_Grip:
  JML $0092C9					;Jump Back to where it normally would be if Sliding more than the Threshold

;Give player strong Turning if on DMag while on Ground, else regular Turning
;Check for DMag and Ground first
Turning_Hijack:  
  LDA $0D51						;Check Machine State
  BMI Regular_Turning			;Branch if Midair
  LDA $0D50						;Load Surface Flags
  BIT #$08						;DMag BIT
  BEQ Regular_Turning   		;Branch if not on a DMag
  LDA #$38						;Load Strong Turning if on DMAg
  BRA Store_Turning				
Regular_Turning:
  LDA $0B20						;Load Current Speed of player
  ASL							;Double it
  LDA $0B21						;Load Hi Byte
  ROL							;Double it (and consider carry from Lo Byte, now Index is Speed / 128)
  CLC
  ADC $EF						;Add Acceleration Index for current Machine
  TAX							;Calculated Index to X
  LDA.l $02C9AB, X			    ;Load Turning depending on Machine and Speed
Store_Turning:
  STA $4203						;Store to Multiplicant B  
  RTL							;Return
 
;Make the Rate at which you redirect your Angle when Strafing stronger, otherwise the straightening effect 
;from grip would be stronger than strafing, therefor not make it possible to strafe on DMag
;Check for DMag and Ground first 
Strafing_Hijack:
  LDA $0D51						;Check Machine State
  BMI Regular_Strafing			;Branch if Midair
  LDA $0D50						;Load Surface Flags
  BIT #$08						;DMag BIT
  BEQ Regular_Strafing  		;Branch if not on a DMag
  LDA #$50						;Load Strong Strafing if on DMAg
  RTL		
Regular_Strafing:  
  LDA $0B20						;Load Current Speed of player
  ASL							;Double it
  LDA $0B21						;Load Current Speed Hi Byte
  ROL							;Double it (and consider carry from Lo Byte, now Index is Speed / 128)
  TAX							;Calculated Index to X
  LDA $02CA23, X				;Load Strafing Acceleration depending on Machine and Speed
  RTL
