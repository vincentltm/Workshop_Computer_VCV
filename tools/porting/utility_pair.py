def post_process(src_content, src_rel):
    src_content = src_content.replace('ComputerCard *cc;', 'ComputerCard *cc = nullptr;')
    # Update mock flash immediately in ProcessSample of Selector so the host can sync in real-time
    src_content = src_content.replace('utilityIndex[i] = ind[i];', 'utilityIndex[i] = ind[i]; g_flash_memory[2093056 + i] = ind[i];')
    return src_content
