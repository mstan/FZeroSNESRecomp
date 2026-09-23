;Makes the amount the player and CPU spin after a direct collision depend on the speed difference

pushpc

org $00BE02                                
  JML advanced_spin_calc	;jump to extra routine for more space
   
pullpc  

;CPU Spin Calculation

advanced_spin_calc:
  PHX                 		;Save Register
  LDX $D1   				;Load Currently used machine

;=========CPU Spin Calculation==========
  
  PHP             			;Save 8 bit mode
  REP #$30        			;Set to 16 bit mode

  LDA $0B20       			;Load speed of player
  SEC             			;Set carry flag before subtraction
  SBC $0B20,Y     			;Subtract CPU speed from player speed

  BCS PositiveResultCPU		;Branch if speed difference positive
  EOR #$FFFF      			;Flip bits if negative...
  INC             			;... add 1 to get positive absolute value

PositiveResultCPU:
  CMP #$0200      			;Compare speed difference to 512
  BCS ApplyNormalCPU		;If speed difference is higher calculate full spin force

  CMP #$0080      			;Compare speed difference to 128
  BCC QuarterSpinCPU 		;If speed difference is lower use lowest spin force

  LDA $FAE5,X     			;Continue with medium spin force if no branch. Load spin force from table
  LSR             			;Half spin force
  STA $0C20,Y       		;Store to current CPU machine
  BRA DoneCPU        		;go to end of CPU calculation

QuarterSpinCPU:				
  LDA $FAE5,X     			;Load spin force from table
  LSR             			;Half spin force
  LSR             			;Half spin force
  STA $0C20,Y       		;Store to current CPU machine
  BRA DoneCPU				;go to end of CPU calculation

ApplyNormalCPU:
  LDA $FAE5,X     			;Load spin force from table
  STA $0C20,Y      			;Store to current CPU machine

DoneCPU:
  SEP #$30        			;Back to 8 bit mode
  PLP

  LDA $0D51,Y         		;Load current CPU machine status
  ORA #$40        			;Set spin flag
  STA $0D51,Y  				;Save it to current CPU machine Status
  
  ;=========Player Spin Calculation==========
    
  PHP             
  REP #$30        			;Set to 16 bit mode

  LDA $0B20       			;Load speed of player
  SEC             			;Set carry flag before subtraction
  SBC $0B20,Y     			;Subtract CPU speed from player speed

  BCS PositiveResultPlayer	;Branch if speed difference positive
  EOR #$FFFF      			;Flip bits if negative...
  INC             			;... add 1 to get positive absolute value

PositiveResultPlayer:
  CMP #$0200      			;Compare speed difference to 512
  BCS ApplyNormalPlayer		;If speed difference is higher calculate full spin force

  CMP #$0080      			;Compare speed difference to 128
  BCC QuarterSpinPlayer		;If speed difference is lower use lowest spin force

  LDA $FADD,X     			;Continue with medium spin force if no branch. Load spin force from table
  LSR             			;Half spin force
  STA $0C20       			;Store to current CPU machine
  BRA DonePlayer       		;go to end of CPU calculation

QuarterSpinPlayer:
  LDA $FADD,X     			;Load spin force from table
  LSR             			;Half spin force
  LSR             			;Half spin force
  STA $0C20       			;Store to current CPU machine
  BRA DonePlayer			;go to end of CPU calculation

ApplyNormalPlayer:
  LDA $FADD,X     			;Load spin force from table
  STA $0C20       			;Store to current CPU machine

DonePlayer:
  SEP #$30        			;Back to 8 bit mode
  PLP						;Pull original Processor Status
  
  LDA #$40     				;Load spin bit
  TSB $0D51					;Apply spin flag to player
 
  JML $00BE27				;Continue with regular spin routine
   