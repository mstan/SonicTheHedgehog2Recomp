import importlib.util
from pathlib import Path
import unittest

spec=importlib.util.spec_from_file_location('custom_smps',Path(__file__).resolve().parents[1]/'tools/assemble_custom_smps.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)

class CustomSmpsTests(unittest.TestCase):
    def test_raw_data_and_relative_labels(self):
        self.assertEqual(module.assemble('Song:\n dc.b $80, -1\n dc.w End-Song\nEnd:\n dc.b 0'),bytes.fromhex('80ff000400'))

    def test_psg_song_header_and_loop(self):
        source='''Song:
 smpsHeaderStartSong 1
 smpsHeaderVoice Voice
 smpsHeaderChan 1,1
 smpsHeaderTempo 1,1
 smpsHeaderDAC DAC
 smpsHeaderPSG PSG,0,0,0,0
DAC:
 smpsStop
PSG:
 dc.b nC4,8,nE4,8
 smpsJump PSG
Voice:
'''
        data=module.assemble(source)
        self.assertEqual(data[:16],bytes.fromhex('00180101010100100000001100000000'))
        self.assertEqual(data[16:],bytes.fromhex('f2b108b508f6fffa'))

    def test_fm_voice_operator_order(self):
        source='''smpsVcAlgorithm 7
smpsVcFeedback 2
smpsVcDetune 1,2,3,4
smpsVcCoarseFreq 1,2,3,4
smpsVcRateScale 0,0,0,0
smpsVcAttackRate 31,31,31,31
smpsVcAmpMod 0,0,0,0
smpsVcDecayRate1 0,0,0,0
smpsVcDecayRate2 0,0,0,0
smpsVcDecayLevel 0,0,0,0
smpsVcReleaseRate 15,15,15,15
smpsVcTotalLevel 0,8,16,24'''
        data=module.assemble(source)
        self.assertEqual(len(data),25)
        self.assertEqual(data[:5],bytes.fromhex('1744332211'))
        self.assertEqual(data[-4:],bytes([24,16,8,0]))

    def test_no_external_actions(self):
        for line in ['include "private.bin"','incbin "private.bin"','jsr $1234','rept 100000','dc.b __import__("os")']:
            with self.subTest(line=line),self.assertRaises(ValueError):module.assemble(line)

    def test_reject_wrong_driver_unknown_symbols_and_overflows(self):
        for line in ['smpsHeaderStartSong 2','dc.b Missing','dc.b 256','dc.w 65536','dc.b 1<<1000000']:
            with self.subTest(line=line),self.assertRaises(ValueError):module.assemble(line)

if __name__=='__main__':unittest.main()
