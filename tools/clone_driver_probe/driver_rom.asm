; Driver software only. Music, effects, instruments and PCM are produced from
; the player's verified ROM by sonic2_sound_assets.c; no game assets go here.
MSUMode = 0
ZeroOffsetOptimization = 1
Snd_driver_RAM = $FFFFF200
Snd_driver_RAM_end = $FFFFF600
v_megadrive = $FFFFFFF8
    cpu 68000
    include "MacroSetup.asm"
disableIntsSave macro
    move.w sr,-(sp)
    ori.w #$700,sr
    endm
enableIntsSave macro
    move.w (sp)+,sr
    endm
    include "sound/Definitions.asm"
    org $100000
ProbeStart:
    dc.b "S2CR"
    dc.l ProbeEnd-ProbeStart
    dc.l SMPS_LoadDACDriver,SMPS_UpdateDriver
    dc.l SMPS_QueueSound1,SMPS_QueueSound2,SMPS_QueueSound3
    dc.l Clone_Driver_RAM,SMPS_RAM.len
    dc.l SMPS_RAM.variables.queue,SMPS_RAM.f_pause
    dc.l MegaPCM_LoadDriver,MegaPCM_LoadSampleTable
    dc.l MegaPCM_DAC_Table
    dc.l Clone_GuardedUpdate
Clone_GuardedUpdate:
    SMPS_UpdateSoundDriver
    rts
; Return one only when the actual Z80 sample loop is idle. The original
; SEGA screen expects its synchronous sample call to finish before timing
; the following hold. Read the driver's protocol under its normal bus grant.
Clone_PCMIdle:
    disableIntsSave
    MPCM_stopZ80
    moveq #0,d0
    move.b (MPCM_Z80_RAM+Z_MPCM_LoopId).l,d0
    MPCM_startZ80
    cmpi.b #Z_MPCM_LOOP_IDLE,d0
    seq d0
    andi.w #1,d0
    enableIntsSave
    rts
    include "sound/engine/Functions.asm"
    include "sound/engine/MegaPCM.asm"

; Runtime fills these descriptors from the verified ROM before initialising.
Placeholder = $230000
Placeholder_End = Placeholder+1
SegaPCM = $270000
SegaPCM_End = SegaPCM+$6174
MegaPCM_DAC_Table:
dac81: dcSample TYPE_PCM,Placeholder,8000
dac82: dcSample TYPE_PCM,Placeholder,8000
dac83: dcSample TYPE_PCM,Placeholder,8000
dac84: dcSample TYPE_PCM,Placeholder,8000
dac85: dcSample TYPE_PCM,Placeholder,8000
dac86: dcSample TYPE_PCM,Placeholder,8000
dac87: dcSample TYPE_PCM,Placeholder,8000
dac88: dcSample TYPE_PCM,Placeholder,8000
dac89: dcSample TYPE_PCM,Placeholder,8000
dac8A: dcSample TYPE_PCM,Placeholder,8000
dac8B: dcSample TYPE_PCM,Placeholder,8000
dac8C: dcSample TYPE_PCM,Placeholder,8000
dac8D: dcSample TYPE_PCM,Placeholder,8000
dac8E: dcSample TYPE_PCM,Placeholder,8000
dac8F: dcSample TYPE_PCM,Placeholder,8000
dac90: dcSample TYPE_PCM,Placeholder,8000
dac91: dcSample TYPE_PCM,Placeholder,8000
dacE0: dcSample TYPE_PCM,SegaPCM,16000,FLAGS_SFX
    dc.w -1
    include "sound/engine/Sonic 2 Clone Driver v2.asm"
ProbeEnd:
