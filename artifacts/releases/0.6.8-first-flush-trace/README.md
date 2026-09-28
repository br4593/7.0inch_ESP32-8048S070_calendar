# Firmware 0.6.8 First RGB Flush Trace

Firmware 0.6.7 still reset inside the first `lv_timer_handler()`. This build
retains the eight-row synchronous RGB flush adapter and adds one-shot serial
markers around its first flush only.

Return the final complete `first RGB flush` line and the reset's `Saved PC`.
If no `first RGB flush entered` line appears, the stall precedes the display
driver callback. If a chunk starts but does not complete, that driver copy is
the blocking operation. If `first RGB flush complete` appears, the stall is
later in LVGL's render pass.

SHA-256:

```text
a4a74c7cdb463ed85f1a22aa02b8651ea8cbd6ec1b4851808b300d12698a3839  firmware.bin
30a84435c314512736bbcf7870676d4e54c4628ffea4b3feaf8012d999fa86fb  firmware.elf
```
