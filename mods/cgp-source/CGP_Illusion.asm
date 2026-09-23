!IS_ILLUSION = $0ADF ; Use single byte free RAM

; EXAMPLE: Enable Illusion on tracks 2, 4, 5 and 12
ILLUSION_ENABLED_TRACKS:
    db 51 ; League 11, Track 1, Rainbowroad
    db 0 ; TERMINATOR, DO NOT CHANGE!

pushpc
org $008976
ILLUSION_HijackLevelLoad:
    JSL ILLUSION_LevelLoad

org $0098F5
ILLUSION_HijackSurfaceCollision:
    JML ILLUSION_SurfaceCollision

org $00BA20
ILLUSION_HijackWallSpinout:
    JSL ILLUSION_WallSpinout
    NOP

org $00E70B
ILLUSION_HijackWallBounce:
    JML ILLUSION_WallBounce
    NOP
pullpc

ILLUSION_LevelLoad:
    ; ORIGINAL CODE
    STZ   $74
    STZ   $E8
    ; ORIGINAL CODE

    LDA   $90     ; Current league
    ASL   A       ; *2
    ASL   A       ; *4
    ADC   $90     ; *5
    ADC   $53     ; Current race number
    INC   A
    STA   $00     ; $00 = league*5 + race + 1

    LDX   #$FF
    - INX
      LDA.l ILLUSION_ENABLED_TRACKS,x
      BEQ   .done
      CMP   $00
      BNE   -

    ; If we got here, we're illusioning

    ; HACKY! Future FZEdit versions will allow to toggle this animation per track
    STZ   $1075   ; Guarantee falling off animation by zeroing this out

    LDA   #$FF
.done
    STA   !IS_ILLUSION ; 00 if not Illusion, FF otherwise
    RTL

; $00 has the surface flags
ILLUSION_SurfaceCollision:
    LDA   !IS_ILLUSION
    BNE   .is_illusion
.not_illusion
    LDA   $00
    BIT   #$10
    BNE   .barrier
	LDA   $00
	BIT   #$20
	BEQ   .magnet
    JML   $0098F9  ; Back to normal processing (check magnet and wall)

.barrier
    JML   $009925  ; Go on with barrier collision processing

; We're illusioning baby!
.is_illusion
    LDA   $00      ; Load surface flags
    AND   #$8C
    BMI   .fall_off
    BEQ   .return
.magnet
    JML   $00992D  ; Go on with magnet collision processing

.fall_off
    LDA   #$40
    TSB   $C3     ; Finish state (exploding)
    LDA   #$06
    STA   $CF     ; Player explosion timer

    LDA   $0D40   ; Current player lap
    STA   $0F38   ; Race lost lap
.return
    JML   $009967

; Do not spin out when driving on a wall if track is Illusion
ILLUSION_WallSpinout:
    LDA   !IS_ILLUSION
    BNE   .is_illusion
.not_illusion
    ; ORIGINAL CODE
    LDA   $0CC0
    AND   #$E0
    ; ORIGINAL CODE
    RTL

.is_illusion
    LDA   #$00  ; Zero wall flags
    RTL

; Do not bounce off walls if track is Illusion
ILLUSION_WallBounce:
    CPX	  #$00				;check if Player
	BNE   .OrigCode			;If not player, just do original Wall Bounce Code even on Illusion
    LDA   !IS_ILLUSION
    BNE   .is_illusion
    ; ORIGINAL CODE
.OrigCode
    LDA   #$20
    BIT   $0CC0,x
    ; ORIGINAL CODE
    JML   $00E710

.is_illusion
    JML   $00E716   ; Return
