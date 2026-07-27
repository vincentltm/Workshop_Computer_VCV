def post_process(src_content, src_rel):
    src_content = src_content.replace('ComputerCard *cc;', 'ComputerCard *cc = nullptr;')
    # Update mock flash immediately in ProcessSample of Selector so the host can sync in real-time
    src_content = src_content.replace('utilityIndex[i] = ind[i];', 'utilityIndex[i] = ind[i]; g_flash_memory[2093056 + i] = ind[i];')
    
    # Zero-division guards for Saw, Square, and SawTri oscillators
    src_content = src_content.replace('return retval/invc;', 'if (invc == 0) invc = 1;\n\t\treturn retval/invc;')
    src_content = src_content.replace('return (retval/invc)>>5;', 'if (invc == 0) invc = 1;\n\t\treturn (retval/invc)>>5;')
    src_content = src_content.replace('retval = retval/invc;', 'if (invc == 0) invc = 1;\n\t\tretval = retval/invc;')
    src_content = src_content.replace('dphase2 /= (524288+p);', 'int32_t _div_a = 524288+p; if (_div_a == 0) _div_a = 1;\n\t\t\tdphase2 /= _div_a;')
    src_content = src_content.replace('dphase2 /= (524288-p);', 'int32_t _div_b = 524288-p; if (_div_b == 0) _div_b = 1;\n\t\t\tdphase2 /= _div_b;')
    return src_content
