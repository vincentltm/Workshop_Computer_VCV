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

    # Zero-division guards for Saw, Square, and SawTri oscillators
    src_content = re.sub(r'invc\s*=\s*phase_incr>>15;', 'invc = phase_incr>>15;\n\t\tif (invc == 0) invc = 1;', src_content)
    src_content = re.sub(r'return\s+retval/invc;', 'if (invc == 0) invc = 1;\n\t\treturn retval/invc;', src_content)
    src_content = re.sub(r'return\s+\(retval/invc\)>>5;', 'if (invc == 0) invc = 1;\n\t\treturn (retval/invc)>>5;', src_content)
    src_content = re.sub(r'retval\s*=\s*retval/invc;', 'if (invc == 0) invc = 1;\n\t\tretval = retval/invc;', src_content)
    src_content = re.sub(r'dphase2\s*/=\s*\(524288\+p\);', 'int32_t _div_a = 524288+p; if (_div_a == 0) _div_a = 1;\n\t\t\tdphase2 /= _div_a;', src_content)
    src_content = re.sub(r'dphase2\s*/=\s*\(524288-p\);', 'int32_t _div_b = 524288-p; if (_div_b == 0) _div_b = 1;\n\t\t\tdphase2 /= _div_b;', src_content)

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
    src_content = re.sub(r'\(x-lastx\)', '((x-lastx != 0) ? (x-lastx) : 1)', src_content)
    src_content = re.sub(r'divisors\[kex\]', '(divisors[kex] != 0 ? divisors[kex] : 1)', src_content)
    src_content = re.sub(r'/\s*sizes\[beatIndex\]', '/ (sizes[beatIndex] != 0 ? sizes[beatIndex] : 1)', src_content)
    src_content = re.sub(r'%\s*sizes\[beatIndex\]', '% (sizes[beatIndex] != 0 ? sizes[beatIndex] : 1)', src_content)
    src_content = re.sub(r'/\s*nChords\[chordtype\]', '/ (nChords[chordtype] != 0 ? nChords[chordtype] : 1)', src_content)
    src_content = re.sub(r'%\s*nChords\[chordtype\]', '% (nChords[chordtype] != 0 ? nChords[chordtype] : 1)', src_content)
    src_content = re.sub(r'/\s*xfadeLen', '/ (xfadeLen != 0 ? xfadeLen : 1)', src_content)
    src_content = re.sub(r'/\s*nSteps', '/ (nSteps != 0 ? nSteps : 1)', src_content)

    return src_content


