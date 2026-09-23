
pushpc

org $009829
    jsl DashPlateFacing_HIJACK1
    nop

org $009842
    jml DashPlateFacing_HIJACK2
    nop


pullpc

DashPlateFacing_HIJACK1:
    lda $0BD1,x ; Facing angle
    sbc $109D,x ; Goal angle
    rtl

DashPlateFacing_HIJACK2:
    LDA $0D51,X			;load which surface current machine is driving on
    BIT #$80
    BNE mid_air	
    lda $109D,x ; Goal angle
    sta $0BE1,x ; Momentum angle
	jml $009847
mid_air:
	jml $00986C
