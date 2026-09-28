# Bounce-buffer rollback after failed 0.6.18 trial

Restores 0.6.17 display source and working weather icons. Build passed:
`/home/barro/.platformio/penv/bin/pio run -e rgb-idf5 -s`.
This rebuilt binary differs from the original 0.6.17 archive; matching ELF
and SHA256SUMS are preserved here. No hardware verification performed.
Known remaining issues: flashing, artifacts and top-to-bottom wrapping.
Upload: `/home/barro/.platformio/penv/bin/pio run -e rgb-idf5 -t upload --upload-port /dev/ttyUSB0`.
