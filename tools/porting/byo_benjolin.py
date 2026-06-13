def post_process(src_content, src_rel):
    if src_rel == "main.cpp":
        target = "runglerOut2 |= ((bits[i] << (2 * (i - (SHIFT_REG_SIZE - RUNGLER_DAC_BITS)))) & 0x3F);"
        replacement = """int shift_amt = 2 * (i - (SHIFT_REG_SIZE - RUNGLER_DAC_BITS));
			runglerOut2 |= (((shift_amt < 0 || shift_amt >= 32) ? 0 : (bits[i] << shift_amt)) & 0x3F);"""
        src_content = src_content.replace(target, replacement)
    return src_content
