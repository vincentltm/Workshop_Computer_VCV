import re

def post_process(src_content, src_rel):
    if not src_rel.endswith("modes.cpp"):
        return src_content

    # Fix LED update throttle: loopCount < 1000 with sleep_ms(2) in Run()
    # means LEDs update every ~2 seconds. Reduce to 5 iterations (10ms).
    src_content = src_content.replace(
        'if (++loopCount < 1000)\n      return;\n    loopCount = 0;',
        'if (++loopCount < 5)\n      return;\n    loopCount = 0;'
    )

    return src_content
