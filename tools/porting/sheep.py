def post_process(src_content, src_rel):
    # `cabs` in Sheep conflicts with <complex.h> cabs (complex-number absolute value).
    # Replace all uses with a safe local name.
    src_content = src_content.replace('cabs(', 'iabs32_(')
    # Inject the helper before its first use. We guard with a define so multi-file
    # compilations don't double-define it.
    guard = '#ifndef IABS32_DEFINED\n#define IABS32_DEFINED\nstatic inline int32_t iabs32_(int32_t x) { return x < 0 ? -x : x; }\n#endif\n'
    # Insert after the last #include at the top
    import re
    # Find position just after the last top-level #include
    m = None
    for m in re.finditer(r'^#include\s+[<"].+[>"]', src_content, re.MULTILINE):
        pass
    if m:
        insert_pos = m.end()
        src_content = src_content[:insert_pos] + '\n' + guard + src_content[insert_pos:]
    else:
        src_content = guard + src_content
    return src_content
