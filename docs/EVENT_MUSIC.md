# Music customization

Mods > Sound contains **Shared Themes**, **Sonic 2 Campaign**, and
**2-Player Competition**. Every track defaults to **Stock**.

Choose songs independently in their dropdowns. Stage rows offer matching
Simon Wai prototype arrangements. Oil Ocean also offers the slow Simon Wai
Casino Night 2P composition and its August 21 version. Prototype playlist
numbers differ from retail; each choice uses the selected composition.
Zones that share one song also share one selector. The special stage, Options
menu and competition menu/results have their own applicable rows.

Shared Themes separates 1-UP, Game Over, Continue, Act Clear, Invincibility,
Drowning, bosses, title, credits, Super Sonic, and the Chaos Emerald
jingle. Applicable rows also offer Sonic 1, Sonic 3, Sonic & Knuckles, and
November 1993 Sonic 3 prototype arrangements. Sonic CD's US soundtrack is
available for Game Over, Act Clear, and Invincibility. Super Sonic and
Invincibility remain separate choices.

September 14, 1992 prototype choices are available for Credits and Emerald
Hill 2-Player. Options also offers Data Select and Competition Menu music.
Unused campaign selectors are omitted in fresh settings. Explicitly saved
older choices retain their original donor and song.

Every used track also offers **Custom music**. Import WAV, Genesis VGM or
Sonic 1 68K SMPS BIN/ASM, and audition the configured soundtrack in
**Options > Music Player**. See [Custom music and Music Player](CUSTOM_MUSIC.md)
for formats, channel isolation and playback controls.

Only the donors needed by selected replacements appear below the dropdowns.
Choose a raw ROM or ZIP. Contents are verified before importing one private
copy, shared with matching tracks and the Sonic 3 family. For CD music, choose
the supported US CUE/BIN, SGCD, or disc ZIP with its audio tracks.

Selections can be prepared before importing their donors. A section can be
enabled once all its selected tracks have verified sources. Importing a
matching source enables the section when its remaining requirements are
ready. Disable a section to restore stock tracks while retaining its choices,
or choose Stock for one row.

Replacements enable Clone Driver automatically. Restart to apply changes.
Music customization is for local play; disable these sections and Clone
Driver, then restart before netplay. Cartridge sound effects remain in use.
Native campaign SRAM and progress are independent of music choices.

Normal Windows releases include the sound importer. Source builds can run the
same tools with Python 3, or build the sonic_sound_importer target with
PyInstaller. ROMs, source audio, samples, private imports and settings are not
distributed.

Prototype references:

- [Sonic 2 Simon Wai prototype](https://hiddenpalace.org/Sonic_the_Hedgehog_2_(Simon_Wai_prototype))
- [Sonic 2 August 21, 1992 prototype](https://hiddenpalace.org/Sonic_the_Hedgehog_2_(Aug_21,_1992_prototype))
- [Sonic 2 September 14 disassembly](https://github.com/desiresin/Sonic-2-9-14-92-Disassembly)
- [Sonic 3 November 3, 1993 prototype](https://hiddenpalace.org/Sonic_the_Hedgehog_3_(Nov_3,_1993_prototype))

The optional mixed-source sequencer uses the software-only
[skdisasm Clone Driver](https://github.com/TheBlad768/skdisasm-clone-driver)
at revision 13ca55d1f9bf2268f440616404cc65a3b08a6ad6. Source music,
instruments, envelopes and percussion are prepared from verified user media.
