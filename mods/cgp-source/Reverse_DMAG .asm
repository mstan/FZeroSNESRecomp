;Allows you to use Up_Magnets that push you up
;Apply the DOWNPULL_MAGNET property in Tiled to the tile BELOW the regular DMag
;Optional: Apply JUMP property to make you go up when you are not yet airborne

pushpc
  
org $009C55
  JML Check_Landing			;Hijack Landing Check to allow negative jump height
  NOP
  NOP
 
org $009C6B                           
  JML Calculate_DMag		;Hijack DMag
  NOP
  NOP
   
pullpc  

Up_Magnet_Height_Gain:
  dw $0007, $0008, $0005
  
Up_Magnet_Gain_Limit:
  dw $0190, $0200, $0120  

End_Fall:
  JML $009C8C						;Go to landing calculation
  
Check_Landing:
  CMP $29							;Compare current Height to Ground Level Height
  BCC End_Fall						;If lower than Ground Level Height, end falling
  CMP #$C000
  BCS End_Fall						;Also end fall if you are very high, in case you go from ground level to negative in 1 frame  
  JML $009C5B						;Continue with regular routine.
  
Calculate_DMag:
  LDA $0CD0,x						;Load Surface ID current player is over
;TODO: Instead of checking for tile ID, make use of custom tile properties!  
  CMP #$00B6						;Compare to the tile below regular DMAG
  BEQ Up_Magnet						;Branch to Up_Magnet if tile ID is the same 
  CMP #$00CC						
  BCC No_Up_Magnet					;No Up Magnet if tile ID was lower than CC	
  CMP #$00D0
  BCC Up_Magnet  				    ;Branch if tile was in range of tile CC to CF
No_Up_Magnet:
  LDA $0BB0,x						;If not, Do regular Down Magnet. Load Altitude of current player
  SBC #$0060						;Subtract 60h from altitude gain
  BRA Apply_Falling					;Return to apply falling routine
Up_Magnet:  
  LDA #$7000						;Load maximum height at which I want you to gain Height
  SEC
  SBC $0BC0,x						;Subtract Height of current racer from it, to make magnet effect smaller when further away.
  BCS Positive_Height				;Keep current result if you didn't go below 0
  LDA #$0000						;If you went below 0, load 0
Positive_Height:
;Next two steps should effectively divide by 256 
  XBA								;Switch Lo and Hi Byte
  AND #$00FF						;Only keep old Hi Byte as new Lo Byte
  LSR								;Divide by 4
  LSR
  STA $4202							;Store Result as Multiplicant A  
  
  CPX #$00							;Check if currently player
  BEQ Check_Player_Tilt				;If player, check player Up/Down presses
  LDA #$0002						;If not player, load index for Down Press 
  BRA Calculate_Mult_B  			;Continue with calculating Height Gain Multiplicant B
Check_Player_Tilt:  
  LDA $0B10							;If player, Load current vehicle tilt (Up/Down) Presses
  AND #$000C						;Mask Up and Down Press Bits
  LSR								;Divide by 2, to get index 0 = NoPresses, 2 = DownPress, 4 = UpPress
Calculate_Mult_B:
  PHY								;Save Y Register
  TXY								;X (Current Player) to Y (I use X for LDA because only that seems to allow LDA.l)
  PHX								;Save X (Current Player) Register
  TAX								;Save Index to X (Tilt/Up/Down)
  LDA.l	Up_Magnet_Height_Gain,x		;Load Height Gain depending on Up/Down presses
  STA $4203							;Store Result as Multiplicant B 
  LDA $0BB0,y						;Load Altitude Gain of current player  
  SEC
  SBC $14							;Subtract gravity
  ADC $4216							;Add Calculated altitude Gain
  CMP.l Up_Magnet_Gain_Limit,x		;Load Limit depending on Up/Down presses				
  BMI Restore_Registers				;If altitude gain is lower than this, continue with apply falling
  LDA.l Up_Magnet_Gain_Limit,x		;Set maximum altitude gain depending on Up/Down presses
Restore_Registers:  
  PLX								;Restore previous X (Current Player) Register
  PLY								;Restore previous Y Register
Apply_Falling:
  JML $009C71						;Return to apply falling routine	
