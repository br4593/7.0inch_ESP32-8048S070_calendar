# 0.6.23 Main UI candidate

**Withdrawn after physical test:** the owner reported that display artifacts
returned with this firmware. The workspace and upload build were rolled back
to the byte-matching 0.6.22 UI layout release. Keep this package only for
diagnosis; use `../0.6.22-ui-layout/` for the previous version.

This candidate follows the 0.6.22 layout update. The accepted 0.6.21 staged
startup remains preserved separately, including its 60-second service delay,
RGB settings and diagnostic logging.

Main event titles now use explicit high-contrast text in both themes, including
after a theme switch. The weather card centers the icon/temperature band,
condition, details and update line. Unavailable conditions use short captions;
the full service detail remains on Forecast. Weather details use three compact
lines so they fit the card.

Build: `/home/barro/.platformio/penv/bin/pio run -e rgb-staged-startup -s`

Upload with the board connected, replacing `/dev/ttyUSB0` if needed:

`/home/barro/.platformio/penv/bin/pio run -e rgb-staged-startup -t upload --upload-port /dev/ttyUSB0`

EEZ Studio 0.28.0 regenerated the UI. The target build and all three existing
host test suites passed. Source and generated widget bounds were reviewed, but
there is no connected board here; light/dark appearance and real-panel weather
balance still need physical confirmation. The previously reported mild
gray-area flicker was outside this UI change.

This directory holds the matching firmware binary and ELF, bootloader,
partitions, board profile, PlatformIO configuration, source archive and
SHA256SUMS. Run `sha256sum -c SHA256SUMS` here to verify the files.
