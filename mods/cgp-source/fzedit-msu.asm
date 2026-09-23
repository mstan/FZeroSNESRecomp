; 2015-06-06: MSU1 asm written originally by Conn
; 2024-01-19: Ported to FZEdit and slightly modified by Khilendel
; 2024-02-11: Rewritten and ported to BS F-Zero Deluxe by Catador

lorom

if read1($00FFD9) == 0 || read1($00FFD9) == 1
  !HOOK_ADDR        = $00F88A
  !HOOK_NO_SFX_ADDR = $00F8AD
  !FREESPACE        = $02C1CC

  if read1($00FFD9) == 0
    print "Patching the Japanese version!"
  else
    print "Patching the American version!"
  endif
elseif read1($00FFD9) == 2
  !HOOK_ADDR        = $00F888
  !HOOK_NO_SFX_ADDR = $00F8AB
  !FREESPACE        = $02C1CE
  print "Patching the European version!"
else
  error "Unsupported ROM region!"
endif

; VANILLA RAM
!ram_APUIO0     = $46
!current_race   = $53
!current_league = $90

; MOD-SPECIFIC RAM
!msu_track    = $0180 ; MSU track (actually lower 3 bits from APUIO0)
!is_busy      = $0181 ; MSU is busy flag
!spc_fallback = $0182 ; SPC fallback (?)
!msu_found    = $0183 ; "S-MSU1" string found on MSU_ID
!volume       = $0184 ; Current playing volume

; READ-ONLY REGISTERS
; References: https://sneslab.net/wiki/MSU1, https://helmet.kafuka.org/msu1.htm
MSU_STATUS    = $2000 ; darptRRR (d=data port busy; a=audio port busy; r=audio repeat flag; p=audio playing flag; m=track missing; R=revision)
MSU_READ      = $2001 ; Data port
MSU_ID        = $2002 ; "S-MSU1" identification string

; WRITE-ONLY REGISTERS
; References: https://sneslab.net/wiki/MSU1, https://helmet.kafuka.org/msu1.htm
MSU_SEEK      = $2000 ; 32-bit offset value
MSU_TRACK     = $2004 ; Audio track (2 bytes)
MSU_VOLUME    = $2006 ; Audio volume
MSU_CONTROL   = $2007 ; Play/repeat audio

; Hook to MSU (normal processing)
org !HOOK_ADDR
    JSL MsuHook
    NOP

; Hook to MSU (sound effects disabled)
org !HOOK_NO_SFX_ADDR
    JSL MsuHook_NoSfx
    NOP

; Hook to MSU Reset
org $008006
    JSL MsuReset
    NOP
    NOP

; FREESPACE!
org !FREESPACE
MsuHook_NoSfx:
    STA   !ram_APUIO0
MsuHook:
    LDA   !msu_found
    BEQ   .spc_write
    LDA   !ram_APUIO0
    AND   #$F8              ; Mask music out, keep only effects
    STA   $2140

    LDA   !ram_APUIO0
    BEQ   .spc_write
    BPL   .has_music
    ; Music is fading, decrease volume every frame...
    LDA   !volume
    SEC
    SBC   #$08
    BCS   +
      LDA   #$00
  + STA   !volume

.has_music
    LDA   !ram_APUIO0
    AND   #$07
    CMP   !msu_track
    BNE   .new_track
    LDA   !spc_fallback
    BNE   .spc_write
.return
    LDA   !volume
    STA.w MSU_VOLUME
.return_no_volume
    LDA   !ram_APUIO0
    RTL

.spc_write
    LDA   !ram_APUIO0
    STA   $2140
    RTL

.new_track
    LDA   !is_busy
    BNE   .check_ready
    JSR   TrackSelector
    STA.w MSU_TRACK+0
    STZ.w MSU_TRACK+1
    STZ.w MSU_CONTROL
    STZ   !spc_fallback
    LDA   #$01
    STA   !is_busy
    LDA   !ram_APUIO0
    RTL

.check_ready
    BIT.w MSU_STATUS
    BVS   .return_no_volume ; Return if audio port is busy (we'll be back here next frame!)
    STZ   !is_busy
    LDA   !ram_APUIO0
    AND   #$07
    STA   !msu_track        ; Set current MSU track
    JSR   LoopSelector
    STA.w MSU_CONTROL
    LDA   #$FF              ; Full volume, change FF->60 when playing in bsnes in case the volume is too loud
    STA   !volume

    LDA.w MSU_STATUS
    AND   #$08
    BEQ   .return           ; This track exists, return

    ; Otherwise, if this track doesn't exist, play on the SPC instead
    STZ   !volume
    STZ.w MSU_VOLUME
    LDA   #$01
    STA   !spc_fallback
    BRA   .spc_write

; Set loop bit for appropriate tracks
LoopSelector:
    LDA   !ram_APUIO0
    AND   #$07
    CMP   #$01              ; Start
    BEQ   .noloop
    CMP   #$02              ; Zoom
    BEQ   .noloop
    CMP   #$03              ; Lost Life
    BEQ   .noloop
    LDA   #$03              ; play+loop bits
    RTS

.noloop
    LDA   #$01              ; play bit
    RTS

; Select appopriate MSU track from APUIO0 value
TrackSelector:
    LDA   !ram_APUIO0
    AND   #$07
    CMP   #$06
    BNE   .return
    LDA   !current_race
    LDX   $58
    BNE   .add10          ; Practice mode, no need to get league*5
    LDA   !current_league
    ASL   A
    ASL   A
    ADC   !current_league
    ADC   !current_race
.add10
CLC
    ADC.b #10
.return
    RTS

MsuReset:
    STZ   $4200             ; ORIGINAL CODE
    STZ   $420B             ; ORIGINAL CODE

    STZ   !volume
    STZ.w MSU_VOLUME
    STZ.w MSU_CONTROL
    STZ.w MSU_TRACK+0
    STZ.w MSU_TRACK+1

    STZ   !msu_track
    STZ   !is_busy
    STZ   !spc_fallback

    STZ   !msu_found        ; MSU not found yet...

    LDX   #$05
    - LDA.w MSU_ID,x
      CMP.l .ID_String,x
      BNE   .done
      DEX
      BPL   -

    ; If we got here, then we found the whole "S-MSU1" string
    INC  !msu_found
.done
    RTL

.ID_String
    DB "S-MSU1"

; Error out if we've blown out of the freespace!
warnpc $02C300
