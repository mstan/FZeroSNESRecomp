;Disallows the Player completing/undoing a lap if they cross the Finish Line while their current checkpoint is not close to the Finish Line
;This prevents completing Laps while doing Shortcuts, which can't always be prevented by a UFO alone

pushpc
  
org $009968
  JML No_Lap_Finish_on_Shortcut 	 ;Hijack Finish Line Check
  NOP
 
pullpc

No_Lap_Finish_on_Shortcut:
  CPX #$00							;Check if current Racer is Player
  BNE Regular_Finish_Routine		;Allow completing/undoing Lap if not Player
  LDA $0D00							;Load current Player Checkpoint
  BEQ Regular_Finish_Routine		;Allow completing/undoing Lap if Player is on first Checkpoint
  CMP $AD							;Compare Player Checkpoint to Max Number of Checkpoints of current Track  
  BEQ Regular_Finish_Routine		;Allow completing/undoing Lap if Player is on last Checkpoint
  JML $0099BC						;Go to Code that sets close to Finish Line to 0, don't allow completing/undoing Lap

;If completing/undoing Lap is supposed to be allowed, go back to regular Finish Line Routine.   
Regular_Finish_Routine:
  LDA $0B71, X						;Do Instructions that were overwritten by the Hijack first. (Load X Chunk of current Racer)					
  CMP $AE							;Compare to Finish Line Chunk				
  JML $00996D						;Go back to regular Routine