# Local release archives

Release binaries, matching ELF debug files, recovery images and source
checkpoints are preserved in this workspace under `releases/` and
`known-good/`. Git ignores these generated release packages because the local
archive contains about 1 GB of debug ELF files. This file documents that policy
and remains tracked; the packages themselves remain untouched on disk.

For a shareable release, build the selected PlatformIO environment, verify the
package checksums, then attach the firmware, matching debug/recovery files and
source snapshot to a GitHub Release.
