def post_process(src_content, src_rel):
    # set_sys_clock_khz is a no-op in VCV (host controls clock)
    src_content = src_content.replace(
        'set_sys_clock_khz(192000, true);', ''
    )
    # SVFLowPass constructor calls tanf(M_PI * f0/48000.0f) — ensure M_PI is defined
    if '#ifndef M_PI' not in src_content:
        src_content = '#ifndef M_PI\n#define M_PI 3.14159265358979323846\n#endif\n' + src_content
    return src_content
