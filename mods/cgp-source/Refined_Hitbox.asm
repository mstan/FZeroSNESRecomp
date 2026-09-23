;Slightly reduces hitbox to race more closely to CPU

pushpc

org $0BBD2		

  CMP #$0016		;Original value #$0018 - Lateral collision hit check
  BCC $0A  
  CMP #$0026		;Original value #$0028 - Direct collision hit check
  
pullpc
  