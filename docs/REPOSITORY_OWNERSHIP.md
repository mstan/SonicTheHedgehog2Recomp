# Sonic 2 repository ownership

Tracking: `beads-3vb.8`. Owner boundary clarified 2026-09-22: reusable opt-in
engine interfaces are allowed; game-specific implementation belongs here.

The approved gameplay/menu checkpoint was pushed as engine `fc57ce247b692642a2eb4db4f3d6a9636ca8c12d`
and consumer `2e014849d3f5dcdbcd33f3973576afa78c9a9fcd`. This follow-up relocates
the implementation without rewriting those commits or changing gameplay.

- `game/`: GameSpec, address/layout/discovery configuration, character and
  companion controllers, donor decoders, campaign/quickstate adapter, menu,
  video adapter and source annotations.
- `game/s2disasm/`: same pinned upstream commit `65ddcc24250af08ddfdf58e36351374ace998e66`.
- `tests/`, `tools/`, `docs/`, `ghidra/annotations/`: game-owned tests, probes,
  feature documentation and reproducible analysis exports.
- Engine `runner/`: hardware runtime, generic save envelope, optional state
  callbacks, audio-queue serialization and diagnostics. It contains no new
  Sonic 2 eligibility/progression/art/AI logic.

CMake passes this repo's absolute `game/` directory to the shared generator.
Legacy relative paths remain supported for unmigrated consumers. Game and
framework CTests are registered independently. ROMs and generated translations
remain ignored; owner builds and save files are not moved or deleted.

Source provenance is retained in the original engine commit and the moved
files. Mechanical moves were SHA-256 checked before path edits. Historical
feature ledgers can mention old engine paths; current paths are those above.

## Validation

Windows Release validation from the caller-owned game directory:

- 23 CTests pass (the original 22 plus absolute/legacy generation-path coverage).
- 36 campaign runtime cases pass.
- 23 quickstate cases pass in both diagnostic and stripped builds.
- Fresh stripped Sonic 1 and Sonic 2 builds match their stock screenshot and
  RAM references over 3,600 frames each; no dispatch misses.
- Archived approved and relocated executables produce identical frame-60 CPU,
  layout and full WRAM snapshots. The checked-in boot baseline fails identically
  on both builds (15 field differences); this pre-existing fixture discrepancy
  is tracked as `beads-5dyp.11`, not concealed by regenerating the baseline.
- The relocated disassembly rebuild matches the owner ROM byte-for-byte and
  exports 2,952 code labels.

Evidence is under ignored `build/ownership-*` directories. Android source paths
were updated mechanically; no Android/NDK build is claimed. The optional cosim
metadata moved to `game/cosim.json`; supply it with `--game s2 --game-config`
to the shared cosim/report tools. Metadata loading was checked without launching
the two-process harness; no cosimulation result is claimed here.

The strict quickstate fingerprint includes source/config inputs. Relocation
can change that fingerprint; keep existing quickstates with their matching
executable. Campaign SRAM stays portable. No production state ID is retagged.

## Remaining scope

P1 Super support for Sonic/Tails/Amy/Knuckles (`beads-5dyp.10`) was separate
from this relocation. Its subsequent game-only implementation, source ledger
and validation are documented in [SONIC2_PARTY_SUPER.md](SONIC2_PARTY_SUPER.md).

Other games' legacy engine directories are explicitly outside this migration.
