# Redraw performance — 0.6.34

Implements the first measured performance step from the [FPS investigation](../fps-investigation-2026-09-28.md), on top of `0.6.33-ui-alignment`. Static EEZ screens and generated UI are unchanged. The handwritten controller avoids unchanged Settings layout, Firmware paint and whole-screen Appearance updates while dragging brightness. Status polling remains 250 ms; countdown and transfer progress remain live, and brightness persistence still occurs after release.

The normal `rgb-staged-startup` image retains the 800 × 8 internal LVGL buffer, 8-row copy chunks, two 800 × 10 RGB bounce buffers, the 12.5 MHz pixel clock, complete board profile and installed framework cache settings. It is identified as `0.6.34-redraw-performance`.

A separate `rgb-staged-startup-buffer16` image, identified as `0.6.34-redraw-buffer16`, changes only the LVGL draw-buffer size to 800 × 16. RGB565 allocation rises from 12,800 to 25,600 bytes (+12,800 internal bytes). Allocation falls back to the normal 8-row buffer if the larger buffer cannot be obtained. Boot output reports the actual allocation. This is an experiment to measure rendering traversal/flush overhead, not a verified FPS gain; the 8-row copy limit and panel scanout remain unchanged.

## Measuring the board

Five-second `[rgb]` lines retain previous counters and add `rendered_frames`, `vsyncs`, `total_copy_us` and `max_frame_copy_us`. These count distinct things:

- `flushes`: partial dirty-area buffer transfers; many transfers can belong to one redraw.
- `rendered_frames`: refreshes with pixels, counted once at the last flush. Empty timer passes do not count. This is UI redraw activity, not tear-free presentation.
- `vsyncs`: panel VSYNC interrupts, aggregated with a short protected counter. The ISR does not log, allocate, call LVGL or release its draw buffer.
- `pixels`: transferred pixel work, including overlapping/partial regions; divide by 384,000 for equivalent full-screen work, not frame count.
- `total_copy_us` / `max_copy_us`: CPU-copy aggregate / largest individual flush duration; `max_frame_copy_us` sums copy time within one redraw.
- `max_handler_us` and `max_service_gap_us`: worst LVGL handler time and gap between service starts; these include more than framebuffer copying.

Rate calculation: `rendered_frames × 1000 / window_ms` is redraws/sec; `vsyncs × 1000 / window_ms` is panel scans/sec. The latter is nominally 22.55 Hz under the unchanged timing. Low/zero redraw activity on an idle screen means retained content is working. Neither counter proves tear-free output or input-to-visible latency. Ignore startup traces and compare stable windows after services start.

Compare normal and buffer16 using the same UI/data/theme, power, cable and brightness. On each, collect three 10–30 second samples: idle Settings, idle Firmware with OTA disabled, and repeatable Week scroll / brightness drag. Then exercise live status, an Armed countdown and OTA progress plus calendar/weather networking. Compare redraw work, copy/handler/gap maxima and `heap` / `largest` internal blocks. Reject a variant that worsens stability, display artifacts or networking headroom. Inspect touch corners, colors and manual brightness restoration on hardware.

No panel clock or cache-line experiment is silently enabled. A 64-byte cache-line comparison requires a matching rebuilt framework/SDK; an app macro would not change the precompiled framework. A 30 Hz scanout needs validated higher pixel-clock timing. Those remain separate physical experiments.
