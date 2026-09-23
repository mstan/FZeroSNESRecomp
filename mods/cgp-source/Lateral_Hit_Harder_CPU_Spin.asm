;After a lateral collision, makes the CPU rotate the opposite direction of the player
;and harder than the player after a lateral collision, as the CPU otherwise almost instantly recovers due to insane turn performance

pushpc

org $00BD1A                           
  JML increase_cpu_spin		;jump to extra routine for more space
  
pullpc 

increase_cpu_spin:  
  LDA $0B20,Y				;Load CPU Speed, they are supposed to rotate harder when faster
  LSR  						;Divide by 2 to not make them rotate too much
  CMP #$0300				;Don't exceed this threshold
  BCC $03 
  LDA #$0300
  ORA $15					;Apply saved rotation
  EOR #$8000				;flip rotation direction bit, to make CPU spin in opposite direction of player
  STA $0C20,Y				;store spin to CPU
  SEP #$20           		;Back to 8-bit mode
  JML $00BD1F				;Continue with regular routine
