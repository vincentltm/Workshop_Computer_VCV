def post_process(src_content, src_rel):
    src_content = src_content.replace('ComputerCard *cc;', 'ComputerCard *cc = nullptr; HeapCardGuard cc_guard(cc);')
    src_content = src_content.replace('selector.Run();', '/* selector.Run(); bypassed in VCV */')
    return src_content
