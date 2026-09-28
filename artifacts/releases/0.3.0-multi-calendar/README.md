# Firmware 0.3.0 Multi-Calendar

Adds four named private-iCal slots to the trusted home-network settings page.
Configured calendars are downloaded sequentially, merged only when every
download succeeds, and displayed with stable source colors. Existing
single-calendar configuration migrates into slot 1. Saved private addresses
are never prefilled or returned.

Upload `firmware.bin` through the local OTA page. Keep `firmware.elf` for panic
backtrace decoding. This build compiled successfully but has not been observed
on the physical display.

SHA-256:

```text
d65ef7440185783f27b9a96fda03aef17174055673f92e3ca53a0a01dfc35d92  firmware.bin
1527cb9fa46d0a146f4559d16da1e73d6a238a1203a06c40f8683b6d0e09b44e  firmware.elf
```
