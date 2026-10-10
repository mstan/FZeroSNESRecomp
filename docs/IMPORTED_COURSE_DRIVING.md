# Driving checks for imported courses

Import validation includes short runs in the actual game. Completing every lap
is unnecessary, but a successful conversion and a finish-line fixture do not
prove that the surfaces work.

For each course, `tests/validate_course_driving.py` boots the normal race and
saves an approach state. It reads the FZEdit map and tile properties to find
repair strips, boundaries, jumps, dash plates and off-course areas. It places
the car before a suitable surface, then releases it to ordinary controller
input and guest physics. It also runs a reproducible sequence of steering,
braking and shoulder inputs from the starting area.

The checks observe movement, sampled terrain, energy recovery and damage,
jump height, airborne state, dash activation and destruction outside the
course. Initial position, speed, heading, energy and checkpoint can be set;
the results and sampled surface flags are never injected. Dash approaches
respect the direction supplied by the native checkpoint code. Repair tests
brake on the strip long enough for the native repair animation to complete.

Run with the owner's private USA ROM and an already imported pack:

```powershell
python tests/validate_course_driving.py --build build --stock '<private-ROM>' `
  --packs-root '<installed-packs>' --pack '<pack-id>' --out captures/driving-check
```

Use a new output directory each time. Repeat `--pack` to test several packs;
`--course` can limit a diagnostic run. The seed is recorded in `validation.json`.
Logs, snapshots, controller sequences, frame traces and screenshots remain
under that private output directory. Do not commit the imported experiments,
source ROMs, audio, snapshots or screenshots.

An absent surface or an unsuitable approach appears under `skipped`, rather
than counting as a passing interaction. These are short driving checks, not
full-lap qualification. Course loading, music, cup progression and persistent
records have separate checks.

## Older native terrain

Maximum Velocity retains the original USA tile classifiers instead of an
extended FZEdit property table. Its separate mine-location bitmap loader is
disabled; its repair, jump, wall and other tile rules remain active. The
converter must translate those verified classifiers into editor properties,
rather than treating the missing bitmap as an empty terrain table.

The conversion requires the donor's classifier, landing and recovery code to
match the verified USA reference. Unrecognized replacements remain unsupported.
Existing Maximum Velocity packs imported with 0.8.2 need removal and reimport
with the corrected converter; updating the executable does not rewrite the
previously reconstructed project.

## 0.8.3 validation

The private validation run checked all 20 Maximum Velocity, 10 Hybrid and 15
ReFractured courses at 21:9. Every course passed repair, barrier-damage and
controller-fuzz checks. Suitable approaches also passed 17 jump, 20 dash and
11 off-course destruction checks: 183 interactions passed, with 87 surface
cases explicitly skipped because no suitable approach was found.

Dash setup failures were rechecked with the native checkpoint direction and
approaches that avoid push tiles. The earlier Petrous Road destruction setup
started on a jump ramp and returned from a short void to terrain; it did not
provide a valid destruction test. That case remains skipped, not a pass.

The original 0.8.2 Maximum Velocity import fails repair, barrier, jump and dash
checks using independent source geometry. The corrected import passes those
checks. Native resource reconstruction, donor music choices, records writes
and restart persistence also pass for all three packs. Starting-grid captures
for all 45 courses and driving captures were inspected for obvious rendering
problems. This does not establish full-lap or every-terrain qualification.

Local evidence is under `captures/driving-three-20261010/qualification.json`,
with per-frame traces, controller scripts and snapshots in its referenced run
directories. Those private files are excluded from commits and player bundles.
