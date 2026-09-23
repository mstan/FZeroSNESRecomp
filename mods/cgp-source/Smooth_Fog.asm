
pushpc

; Use softer shading for the top 3 pixels of the fog, which touches the horizon graphics
org $08EF9F ; Dark fog, first value is the top pixel of the fog. Use numbers starting at $60 (no fog) up to $7E 
	DB $61, $63, $68

org $08EFCC ; Bright fog, first value is the top pixel of the fog. Use numbers starting at $E0 (no fog) up to $FF 
	DB $E1, $E4, $E8

pullpc

