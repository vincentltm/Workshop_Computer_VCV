import re

def post_process(src_content, src_rel):
    if src_rel == "src/Glitter.cpp":
        # Replace the ambiguous abs() call on uint64_t subtraction
        src_content = src_content.replace(
            "abs(clockCount_ - samplesPerPulse_)",
            "std::abs(static_cast<int64_t>(clockCount_) - static_cast<int64_t>(samplesPerPulse_))"
        )
        # Also replace abs(grainSample) with std::abs(static_cast<int>(grainSample)) to avoid any float ambiguity
        src_content = src_content.replace(
            "abs(grainSample)",
            "std::abs(static_cast<int>(grainSample))"
        )
    return src_content
