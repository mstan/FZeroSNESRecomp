;Makes you bounce away less strongly from the CPU on lateral collisions

pushpc

org $00BC6D                            
  LDA #$0A		;set bounce amount to #$0A
  NOP			;don't subtract how far player overlaps with CPU car to have always same bounce strength
  NOP
  NOP
  
pullpc 
  

  