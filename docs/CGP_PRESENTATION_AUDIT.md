# CGP presentation and stock BS isolation

CGP energy boost now uses the author's single S indicator beside the power
bar. The previously omitted twelve-byte OAM layout at `$0B:EC94` hid two
stock S-jets and moved the remaining indicator. Keeping three stock icons
was misleading for a regenerating, energy-consuming boost. The assembled
layout is now included only in images for CGP-tuned player identities.
Original cars and all eight stock BS cars retain their native S-jets.

The common POWER label, speed/rank layout, widescreen anchors and visibility
hooks remain owned by the canonical renderer. A vehicle pack does not import
the donor's lightning-bolt label, lower-left branding or executable. Exhaust
uses the selected identity's matching complete source table; the retired
independent tuning/boost/exhaust selectors cannot retune a stock BS car.

The owner restored both reported WebP files, and private reference copies
were preserved. The first depicts noisy Mute City scenery with the donor's
bolt/S/branding presentation. The second marks the native HUD and fragments
beside a BS ship on Marine City. The files alone do not establish the capture
application or exact settings. They must not be described as matched captures
of the current build.

Fresh donor and canonical-engine captures contain the same authored Mute
City mosaic and Marine City road patterns. Nine Marine City captures cover
three cohorts in native, enhanced 4:3 and 21:9. Every road-graphics byte and
all 16,384 streamed map cells match the decoded course data. The entire sky
graphics region matches in those nine and four additional Mute City captures.
This did not reproduce source-data corruption. It does not prove that every
unknown configuration in the original screenshots was correct.

`tests/validate_vehicle_presentation.py` checks all eight BS selections with
CGP vehicle settings absent versus conflicting settings: complete final WRAM
and frames are identical. It also checks stock, untuned catalog, Great Star
and White Cat boost-icon placement, and renders their captures in 4:3, 21:9
and 4x HD. The resulting frames were inspected. Earlier twelve-identity
selector checks have been superseded by the restored native carousel checks
described in `CGP_VEHICLE_RUNTIME.md`. Both selected and dim palette resources
now come from each identity's authored source.

Private evidence: `captures/feedback-20260923/{reported-images,visual-sources,
presentation,p1-reference,vehicle-selector}`. Screenshots and ROM-derived
resources are not included in the repository or release.
