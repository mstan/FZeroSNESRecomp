;Makes you turn away from the CPU on lateral hits, instead of turning into them on some occasions

pushpc

org $0BCAB			;when player is on the LEFT and FASTER than CPU
  LDA #$8000		;load ROTATE LEFT Bit
 
org $0BCB9 			;when player is on the RIGHT and FASTER than CPU
  LDA #$0000		;Disable ROTATE LEFT, rotate to the right instead
  
org $0BCE3			;when player is on the LEFT driving head on towards CPU
  LDA #$8000		;load ROTATE LEFT Bit
 
org $0BCF1 			;when player is on the RIGHT driving head on towards CPU
  LDA #$0000		;Disable ROTATE LEFT, rotate to the right instead
  
pullpc
  