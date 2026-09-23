
pushpc

;Hijack Coloring of Track Name depending on Difficulty Level
org $00AC15
  JSL Color_Track_Name_Routine
  RTS

;Table for Track Name Colors  
org $00AC1D
  db $31, $31, $35, $33, $37  			;$31 = Green, $33 = Red, $35 = Yellow, $37 = Blue
										;First value for Beginner, last value for Legend

;Make CPU go faster on laps 2 - 5 for any track instead of using tables for each Track that are somewhat arbitrary
org $00D432
  LDA #$1E								;Load Bit for Lap 2, 3, 4 and 5
  STA $00								;Store it to $00 for later use
 
;TO DO! Stuff below is a bit strange. FZEdit has modified Explosive Bumper Frequency Code. 
;It would be better to just modify the Hijack_Explosive_Frequency Routine itself for the Difficulty Check.
;But I don't want to change the F-Zero_Final.asm
;Instead I change !Difficulty before the Explosive Routine, so it loads the value I want it to

;Make the explosive bumper interval still use Expert values on higher difficulties  
  LDA $57								;Load Current Difficulty
  PHA									;Save Current Difficulty
  CMP #$02								;Check whether Difficulty was higher than Expert
  BCC Explosive_Routine_Jump			;If not, skip next step
  LDA #$02 								;If higher, then use Expert Index
  STA $57								;Store it to Difficulty, so the Bumper Routine uses #02 as maximum
  
Explosive_Routine_Jump:	
  JSL Hijack_Explosive_Frequency		;Load Explosive Bumper frequency for this Track/Difficulty to RAM
  PLA									;Get Difficulty's original value
  STA $57								;Save it to Difficulty  
  rep 4 : NOP							;Delete Leftovers from Vanilla Code
  
;For Practice Mode, CPU Top Speeds are stored in a single Byte. Bits 6 - 7 for Difficulty, Bits 0 - 3 for finer adjustments
;Usually it ROLs three times to get Bits 6 - 7 to Bits 0 - 1 and then stores it to Difficulty
;Since we have more difficulty levels now, we use bits 4 - 6 and LSR 4 times to shift them to bits 0 - 2 
org $00D556
  LSR									
  LSR
  LSR
  LSR
  NOP									;Delete Leftovers from Vanilla Code
  
  
org $00D568
  LDA.w Checkpoint_Speed_Tables_Main, x	;Tables moved, Load new address
  
org $00D580
  LDA.w Top_Speed_Tables_Main, x		;Tables moved, Load new address

org $00D598
  LDA.w Top_Speed_Tables_Generic, x		;Tables moved, Load new address   

org $00D5AF
  LDA.w Turning_Tables_Main, x			;Tables moved, Load new address   

org $00D5B8
  LDA.w Turning_Tables_Generic, x		;Tables moved, Load new address 

org $00D5C1
  LDA.w Turning_Tables_Lapped, x		;Tables moved, Load new address  

org $00D5CE
  LDA.w Warping_Distance_Tables, x		;Tables moved, Load new address 

;Hijack Routine that stores the acceleration table index of Blue Falcon to Purple Snail at the start of the race
org $0D410  
  JSL Load_Acc_Index_Starter_Snail
  NOP									;Delete Leftovers from Vanilla Code

;Hijack Routine that stores the acceleration table index of Blue Falcon to CPU, after they passed the last "Straight" Checkpoint
org $0E334
  JSL Load_Acc_Index_Main_CPU  
  NOP									;Delete Leftovers from Vanilla Code
  
;Hijack Routine that stores the acceleration table index of Blue Falcon to the CPU that replaces the player after a race
org $0F077
  JSL Load_Acc_Index_Race_Finish
  NOP									;Delete Leftovers from Vanilla Code  
  
;Hijack Routine for finishing a lap, so I can lower the acceleration index for CPU after lap 1 to make them accelerate faster (So they can keep up with player boost)
org $099A9
  STA $0D40, X							;Pull some vanilla code forward, storing player lap counter 
  STA $0F80, X
  JSL Update_Acc_Index_2

;Hijack Routine for Updating player Rank, so acceleration index can be changed depending on Rank (Rubberbanding mechanic)
org $0E9A9
 JSL Update_Acc_Index_1	
  NOP									;Delete Leftovers from Vanilla Code  
  NOP  
  
org $02CAD6

;New CPU Acceleration Table that is a bit longer, to allow me to further change the acceleration index depending on things like difficulty
CPU_Acceleration_Table:
  db 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 32, 32, 32, 32, 16, 10, 6, 4, 3, 2, 1, 1, 1, 1, 1, 1, 0, 0, 0  
  
org $02FB20

Top_Speed_Tables_Main:
  db $70, $70, $71, $72, $73, $73, $75, $78			;Beginner Top Speeds
  db $7A, $7B, $7B, $7C, $7D, $7E, $80, $83			;Standard Top Speeds
  db $83, $83, $84, $85, $86, $87, $89, $8D			;Expert Top Speeds
  db $8A, $8B, $8C, $8D, $8E, $8F, $91, $95			;Master Top Speeds
  db $90, $91, $92, $93, $94, $95, $97, $9B			;Legend Top Speeds
  
Checkpoint_Speed_Tables_Main:  
  db $3C, $53, $66						;Beginner Checkpoint Speeds 0 - 2
  db $42, $5B, $6F						;Standard Checkpoint Speeds 0 - 2
  db $48, $62, $77						;Expert Checkpoint Speeds 0 - 2
  db $4C, $68, $7E						;Master Checkpoint Speeds 0 - 2
  db $50, $6C, $84 						;Legend Checkpoint Speeds 0 - 2

Top_Speed_Tables_Generic:  
  db $3B, $52, $64, $6E					;Beginner Fast Green/Purple Speeds
  db $3A, $51, $63, $6D					;Beginner Slow Green/Purple Speeds
  db $2D, $3F, $4E, $56					;Beginner Fast Brown/Explosive Speeds
  db $2B, $3C, $4A, $52					;Beginner Slow Brown/Explosive Speeds
  
  db $41, $5A, $6E, $78					;Standard Fast Green/Purple Speeds
  db $40, $58, $6C, $76					;Standard Slow Green/Purple Speeds
  db $32, $46, $56, $5E					;Standard Fast Brown/Explosive Speeds
  db $2F, $42, $51, $5A					;Standard Slow Brown/Explosive Speeds
  
  db $47, $61, $76, $81					;Expert Fast Green/Purple Speeds
  db $46, $5F, $74, $7F					;Expert Slow Green/Purple Speeds
  db $36, $4B, $5D, $66					;Expert Fast Brown/Explosive Speeds
  db $33, $47, $58, $61					;Expert Slow Brown/Explosive Speeds
  
  db $4B, $66, $7D, $88					;Master Fast Green/Purple Speeds
  db $4A, $65, $7B, $86					;Master Slow Green/Purple Speeds
  db $3A, $50, $62, $6C					;Master Fast Brown/Explosive Speeds
  db $37, $4C, $5D, $66					;Master Slow Brown/Explosive Speeds
  
  db $4F, $6B, $82, $8E					;Legend Fast Green/Purple Speeds
  db $4E, $69, $80, $8C					;Legend Slow Green/Purple Speeds
  db $3D, $54, $66, $70					;Legend Fast Brown/Explosive Speeds
  db $39, $4F, $61, $6B					;Legend Slow Brown/Explosive Speeds

Turning_Tables_Main:  
  db $02, $0D, $15, $1D 				;Beginner Main Car Turning
  db $02, $0E, $16, $1E 				;Standard Main Car Turning
  db $02, $10, $18, $21   				;Expert Main Car Turning
  db $02, $12, $1A, $24 				;Master Main Car Turning
  db $02, $14, $1C, $27					;Legend Main Car Turning	

Turning_Tables_Generic:  
  db $02, $0C, $14, $1D 				;Beginner Green/Purple Car Turning
  db $02, $0D, $15, $1E 				;Standard Green/Purple Car Turning
  db $02, $0F, $17, $20 				;Expert Green/Purple Car Turning
  db $02, $11, $19, $22 				;Master Green/Purple Car Turning	
  db $02, $13, $1B, $24					;Legend Green/Purple Car Turning
 
Turning_Tables_Lapped: 
  db $02, $0B, $13, $1C 				;Beginner Brown/Explosive Car Turning	
  db $02, $0D, $14, $1D 				;Standard Brown/Explosive Car Turning	
  db $02, $0E, $16, $1E 				;Expert Brown/Explosive Car Turning	
  db $02, $10, $18, $20 				;Master Brown/Explosive Car Turning	
  db $02, $12, $1A, $22					;Legend Brown/Explosive Car Turning	
  
Warping_Distance_Tables:
  db $00, $05, $00, $08, $00, $19, $00, $64			;Beginner Warping Distances
  db $00, $04, $00, $06, $00, $14, $00, $64			;Standard Warping Distances
  db $00, $03, $00, $05, $00, $0F, $00, $64			;Expert Warping Distances
  db $00, $02, $00, $03, $00, $0A, $00, $64			;Master Warping Distances
  db $00, $01, $00, $02, $00, $08, $00, $64			;Legend Warping Distances
 
;Practice Mode CPU Top Speed Settings
org $02FFDA
  db $48, $38, $28, $18 				;BF, WG, GF, FS. 0X = Beginner, 1X = Standard, 2X = Expert, 3X = Master, 4X = Legend
										;X0 = First Value from "Top_Speed_Tables_Main" of select Difficulty. X2 = 2nd value, X4 = 3rd value, X6 = 4th value, X8 = 5th value
										;Note: Depending on the Player RANK or Lap, it can still pick a higher Top Speed value
  
;While pressing "Select" on Difficulty Selection menu, see whether you are at the end of the list "Legend" (#$04) to loop back to Beginner
org $038819
  CMP #$04					

;While pressing "Down" on the Difficulty Selection menu, see whether you are at the end of the list "Legend" (#§04) to not scroll further
org $03882A
  CMP #$04
  
;Do not make master mode use the same tables as Expert mode, frees up $0F3D which is used for Master Mode Flag 
;Use the free space to load lives depending on difficulty level instead 
org $038841
  PHX							;Save X (Current League?)
  TAX							;A to X (Difficulty Level)
  LDA.l Starting_Lives_Table, x ;Load Starting lives depending on Difficulty
  STA $59						;Store to lives
  PLX							;Restore previous X 
  
;========= Creating the Words for Difficulty Selection like "Beginner" for the Menu ==========   
  
org $0389E1  
  dw $89EB, $89F3, $89FB, $8A03, $8A0B						;Pointers to the 5 Tables below
  
;Following Tables are for creating the Words in the Difficulty Selection Menu. $B0 For example is the X Pos where the word starts  
  db $B0, $06
  dl Word_Beginner				;Pointer to the Word Tiles for "Beginner"
  db $00, $10, $00
  
  db $B0, $06
  dl Word_Standard				;Pointer to the Word Tiles for "Standard"
  db $00, $10, $00

  db $B0, $06
  dl Word_Expert				;Pointer to the Word Tiles for "Expert"
  db $00, $10, $00

  db $B0, $06
  dl Word_Master				;Pointer to the Word Tiles for "Master"
  db $00, $10, $00

  db $B0, $06
  dl Word_Legend				;Pointer to the Word Tiles for "Legend"
  db $00, $10, $00  
  
pullpc  

;Handle Coloring of Track Names depending on Difficulty Level
Color_Track_Name_Routine:
  LDA $58						;Check whether you are in Practice Mode
  BNE Color_Track_Name			;If it is practice mode, branch to color Track name
  LDY $57						;If it is NOT practice mode, load Difficulty Setting to Y
Color_Track_Name:  
  LDA $AC1D, Y					;Load Color Settings from Table depending on current Difficulty Level
  STA $0482						;Store to RAM
  RTL							;Return

;The tiles for the Words are located at $EE800. 1 Tile uses 18h Bytes. For example dw $08BF will use the tile at EE800 + BF * 18h = $EF9E8
Word_Beginner:
  dw $08BF, $08C6, $08C8, $08CA, $08CE, $08CE, $08C6, $08D3		;BF = B, C6 = E, C8 = G, CA = I, CE = N, D3 = R	
 
Word_Standard:
  dw $08D4, $08D5, $08AF, $08CE, $08C5, $08AF, $08D3, $08C5		;D4 = S, D5 = T, AF = A, CE = N, C5 = D, D3 = R
  
Word_Expert:
  dw $08C6, $08D9, $08D1, $08C6, $08D3, $08D5, $08FF, $08FF		;C6 = E, D9 = X, D1 = P, D3 = R, D5 = T, FF = Empty
  
Word_Master:
  dw $08CF, $08AF, $08D4, $08D5, $08C6, $08D3, $08FF, $08FF		;CF = M, AF = A, D4 = S, D5 = T, C6 = E, D3 = R, FF = Empty
  
Word_Legend:
  dw $08CD, $08C6, $08C8, $08C6, $08CE, $08C5, $08FF, $08FF		;CD = L, C6 = E, C8 = G, CE = N, C5 = D, FF = Empty
  
Starting_Lives_Table:
  db 07, 06, 05, 04, 03  
 
;Give the CPU diffierent acceleration depending on difficulty. 
;To achieven this, I simply lower the index for the acceleration table on higher difficulties
;This way, they will keep the strong acceleration of lower speeds for longer
Load_Acc_Index_Main_CPU:
  LDA #$9A						;#$94 would be the start of the acceleration table. +4 so that after subtracting highest difficulty you are at the start of the table, +2 so player can keep up for lap 1 when they have no boost yet
  SEC							;Set Carry for Subtraction
  SBC $57						;Subtract Difficulty, so that higher difficulties will use higher acceleration values from the table
  STA $0D71, x					;Store to Acc_Table_Machine_Index for current CPU
  RTL
  
Load_Acc_Index_Starter_Snail:
  LDA #$9A						;Same as above
  SEC
  SBC $57						;Make purple snail acceleration also depend on Difficulty
  STA $0D79						;Store to acceleration index for the starter purple Snail
  RTL
  
Load_Acc_Index_Race_Finish:
  LDA #$99						;Load Index
  SEC
  SBC $57
  STA $0D73						;Store to all CPU Racers
  STA $0D75	
  STA $0D77	
  STA $0D79	
  STA $0D7B						;Also to the CPU that replaces the player after finishing a race
  RTL				

;Make CPU also accelerate faster on laps 2 - 5 and when the player performs well
Update_Acc_Index_1:
  STA $0DC8						;Store calculated Rank to player Rank
  DEC $0DC9						;Probably a "Rank has been updated" flag?  
Update_Acc_Index_2:     
  LDA $0D40						;Load current Lap of player  
  BEQ Skip_Loading_Acc_Index	;Skip Loading index if you are either in lap 1 or lap "0" (I don't want CPU to rubberband on lap1, otherwise having a good acceleration would be bad for the player, as the player being 1st would make the cpu faster)
  BMI Skip_Loading_Acc_Index	;BMI because lap count is FF before crossing the start line  
  LDA $0DC8						;If player is in lap 2 or higher, load player Rank
  DEC							;DEC so A is 0 if Rank was 1
  BEQ Add_Base_Index			;Don't change index if player is in 1st   
  CMP #$04		
  BCC Add_1_to_Index			;Branch to index +1 if if player is Rank 2-4
  LDA #$02						;If player Rank is lower than 4, load index +2
  BRA Add_Base_Index			
Add_1_to_Index:
  LDA #$01						
Add_Base_Index:  
  CLC							
  ADC #$98						;Add base index of #$98 (#$94 is the start of the acceleration table, add +4 so subtracting difficulty won't get index below #$94)
  SEC
  SBC $57						;Subtract Difficulty
  STA $0D73						;Store to all CPU Racers
  STA $0D75	
  STA $0D77	
  STA $0D79	
Skip_Loading_Acc_Index:
  RTL
