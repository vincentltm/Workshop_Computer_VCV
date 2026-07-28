import os
import re

def post_process(src_content, src_rel):
    src_content = src_content.replace('ComputerCard *cc;', 'ComputerCard *cc = nullptr;')
    # Update mock flash immediately in ProcessSample of Selector so the host can sync in real-time
    src_content = src_content.replace('utilityIndex[i] = ind[i];', 'utilityIndex[i] = ind[i]; g_flash_memory[2093056 + i] = ind[i];')
    
    # Inline Saw.h dynamically if included so transformation applies to Saw class without altering source deps
    saw_h_path = os.path.join(os.path.dirname(__file__), '../../deps/external/utility_pair_singlecard/src/Saw.h')
    if os.path.exists(saw_h_path):
        with open(saw_h_path, 'r') as f:
            saw_h_code = f.read()
        src_content = src_content.replace('#include "Saw.h"', saw_h_code)

    # ExpVoct & Exp4000 bounds guards (prevent negative array indexing and 0 frequency return)
    src_content = src_content.replace(
        'int32_t ExpVoct(int32_t in)\n{',
        'int32_t ExpVoct(int32_t in)\n{\n\tif (in < 0) in = 0;'
    )
    src_content = src_content.replace(
        'int32_t Exp4000(int32_t in)\n{',
        'int32_t Exp4000(int32_t in)\n{\n\tif (in < 0) in = 0;'
    )

    # Zero-division guards for Saw, Square, and SawTri oscillators
    src_content = re.sub(r'invc\s*=\s*phase_incr>>15;', 'invc = phase_incr>>15;\n\t\tif (invc == 0) invc = 1;', src_content)
    src_content = re.sub(r'return\s+retval/invc;', 'if (invc == 0) invc = 1;\n\t\treturn retval/invc;', src_content)
    src_content = re.sub(r'return\s+\(retval/invc\)>>5;', 'if (invc == 0) invc = 1;\n\t\treturn (retval/invc)>>5;', src_content)
    src_content = re.sub(r'retval\s*=\s*retval/invc;', 'if (invc == 0) invc = 1;\n\t\tretval = retval/invc;', src_content)
    src_content = re.sub(r'dphase2\s*/=\s*div(\d);', r'if (div\1 == 0) div\1 = 1;\n\t\t\tdphase2 /= div\1;', src_content)

    # Selector divisor guard
    src_content = src_content.replace(
        'const int divisor = 4095/(numUtilities-1);',
        'const int divisor = (numUtilities > 1) ? (4095 / (numUtilities - 1)) : 1;'
    )
    src_content = src_content.replace(
        'ind[0] = KnobVal(Knob::X)/divisor;',
        'int _div_sel = (divisor > 0) ? divisor : 1;\n\t\tind[0] = KnobVal(Knob::X)/_div_sel;'
    )
    src_content = src_content.replace(
        'ind[1] = KnobVal(Knob::Y)/divisor;',
        'int _div_sel2 = (divisor > 0) ? divisor : 1;\n\t\tind[1] = KnobVal(Knob::Y)/_div_sel2;'
    )

    # Math/utility division guards
    src_content = src_content.replace('octaveOffs=0; last_baseNote=0;', 'octaveOffs=0; last_baseNote=0; chordtype=0; lasts=Switch::Middle;')
    src_content = src_content.replace('notes[chordtype]', 'notes[chordtype & 7]')
    src_content = re.sub(r'nChords\[chordtype\]', 'nChords[chordtype & 7]', src_content)
    src_content = re.sub(r'/\s*\(diff\s*-\s*last_diff\)', '/ ((diff - last_diff != 0) ? (diff - last_diff) : 1)', src_content)
    src_content = re.sub(r'\(x-lastx\)', '((x-lastx != 0) ? (x-lastx) : 1)', src_content)
    src_content = re.sub(r'divisors\[kex\]', '(divisors[kex] != 0 ? divisors[kex] : 1)', src_content)
    src_content = re.sub(r'/\s*sizes\[beatIndex\]', '/ (sizes[beatIndex] != 0 ? sizes[beatIndex] : 1)', src_content)
    src_content = re.sub(r'%\s*sizes\[beatIndex\]', '% (sizes[beatIndex] != 0 ? sizes[beatIndex] : 1)', src_content)
    src_content = re.sub(r'/\s*nChords\[chordtype\s*&\s*7\]', '/ (nChords[chordtype & 7] != 0 ? nChords[chordtype & 7] : 1)', src_content)
    src_content = re.sub(r'%\s*nChords\[chordtype\s*&\s*7\]', '% (nChords[chordtype & 7] != 0 ? nChords[chordtype & 7] : 1)', src_content)
    src_content = re.sub(r'/\s*xfadeLen', '/ (xfadeLen != 0 ? xfadeLen : 1)', src_content)
    src_content = re.sub(r'/\s*nSteps', '/ (nSteps != 0 ? nSteps : 1)', src_content)

    return src_content




