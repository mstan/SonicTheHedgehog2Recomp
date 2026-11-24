# Native sound driver

Sonic 2 sequences its stock music, sound effects and imported arrangements
on the host. The runtime reads the game's sound queue at its original update
point and sends timed FM, PSG and PCM output to the Genesis mixer.

Select the verified **Sonic the Hedgehog 2 (World) (Rev A)** ROM in the
launcher. The runtime extracts the sound data from the original, read-only
image. [Music customization](EVENT_MUSIC.md) uses the same sound driver.

Current-build save states and rewind capture playback position and playback
configuration. Music customization requires the same source media and
choices when loading a state. Campaign SRAM is independent of audio settings.
