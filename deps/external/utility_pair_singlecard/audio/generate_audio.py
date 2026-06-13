import os
import csv

# class name, speech text

cards = [ ("delay",        "Delay"),
          ("euclidean",    "Euclidean rhythms"),
          ("bitcrush",     "bitcrusher"),
          ("attenuvert",   "attenuverter"),
          ("clockdiv",     "clockdivider"),
          ("slopesplus",   "slopes plus"),
          ("maxrect",      "rectifier"),
          ("chords",       "chords"),
          ("windowcomp",   "window comparator"),
          ("karplusstrong","karplus strong"),
          ("cross",        "cross switch"),
          ("cvmix",        "CV mixer"),
          ("vco",          "VCO"),
          ("chorus",       "chorus"),
          ("wavefolder",   "wavefolder"),
          ("lpg",          "lowpass gate"),
          ("sandh",        "sample and hold"),
          ("quantiser",    "quantiser"),
          ("bernoulli",    "ber-Nooy gate"),
          ("supersaw",     "supersaw"),
          ("vca",          "VCA"),
          ("slowlfo",      "slow LFO"),
          ("turing185",    "turing one-eight-five"),
          ("glitch",       "glitch") ]

cards = sorted(cards)
piper_models = ('/home/chris/downloads/2025/piper/en_GB-jenny_dioco-medium.onnx --length_scale 2.0 --sentence_silence 0.4',
                    '/home/chris/downloads/2025/piper/en_GB-alan-medium.onnx --length_scale 1.5 --sentence_silence 0.4')

#python_exe = 'python3'
python_exe = '/home/chris/bin/anaconda3/bin/python3'

python_wizard = "/home/chris/projects/rp2040/own/music_thing_computer/template/private_examples/talkie_pcm/python_wizard/python_wizard"
#python_wizard = "/home/chris/projects/python_wizard/python_wizard"

def make_lpc_audio(speech_text, model_num, name):
    c_array_name = name + '_' + str(model_num)
    piperstr = f'echo "{speech_text}" | piper -m {piper_models[model_num]} -f {c_array_name}.wav'
    waitstatus = os.system(piperstr)
    exitcode = os.WEXITSTATUS(waitstatus)
    if exitcode != 0:
        raise RuntimeError(c_array_name+' '+piperstr)

    waitstatus = os.system(f'{python_exe} "{python_wizard}" -T tms5220 -S -p -f C {c_array_name}.wav >> audiodata.h')
    exitcode = os.WEXITSTATUS(waitstatus)
    if exitcode != 0:
        raise RuntimeError(c_array_name)

def generate_names():
  

    for model_num, model in enumerate(piper_models):
        for classname, speechtext in cards:
            waitstatus = os.system(f'echo "{speechtext}" | piper -m {model} -f {classname}.wav')
            exitcode = os.waitstatus_to_exitcode(waitstatus)
            if exitcode != 0:
                raise RuntimeError(classname)

            waitstatus = os.system(f'{python_exe} "{python_wizard}" -T tms5220 -S -p -f C {classname}.wav | sed "s/char /char name_{model_num}_/" >> audiodata.h')
            exitcode = os.waitstatus_to_exitcode(waitstatus)
            if exitcode != 0:
                raise RuntimeError(classname)


def generate_instructions():
    with open('utility_pair_functions.csv') as f:
        csvreader = csv.reader(f)
        instrs = {}
        
        for i,line in enumerate(csvreader):
            if (i==0):
                headers = line
            else:
                items = line
                d = dict(zip(headers, items))

                for model_num, model in enumerate(piper_models):
                    st = ''
                    def add_instruction(txt, knobname=None):
                        if knobname is None:
                            knobname = txt
                        nonlocal st
                        if (len(d[txt])>0):
                            st += knobname+': ' + d[txt]+'. ';

                    if (len(d['XY'])>0):
                        if model_num == 0:
                            st += 'Knob X: '
                        else:
                            st += 'Knob Y: '
                        st += d['XY']+'. '

                    if (len(d['Main'])>0 and (model_num == 0 or len(d['Switch']))==0):
                        add_instruction('Main','Main knob')

                    if model_num == 1 and len(d['Switch'])>0:
                        add_instruction('Switch', 'Switch zed')

                    add_instruction('Audio in')
                    add_instruction('CV in')
                    add_instruction('Pulse in')
                    add_instruction('Audio out')
                    add_instruction('CV out')
                    add_instruction('Pulse out')

                    make_lpc_audio(st, model_num, "instr_"+d['Module'])

        
def generate_global_code():
    print(f'unsigned numUtilities = {len(cards)};')
    print(f'const unsigned char * utility_names[{len(piper_models)}][{len(cards)}] = '+'{',end='')

    for model_num, model in enumerate(piper_models):
        print('{',end='')
        for c in cards:
            print(f'name_{model_num}_{c[0]}, ',end='')
        print('},',end='')
    print('};')

def generate_selection_code():
    for i1,c1 in enumerate(cards):
        for i2,c2 in enumerate(cards):
            print(f'if (utilityIndex[0] == {i1} && utilityIndex[1] == {i2}) cc = new UtilityPair<{c1[0]}, {c2[0]}>;')

#os.system('rm audiodata.h')

#generate_instructions()
#generate_names()

#generate_selection_code()

#generate_global_code()

