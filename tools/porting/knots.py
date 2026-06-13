import re
import os

# Anonymous-namespace constants that appear in more than one .cpp and will clash
# in a unity build. We rename each occurrence using the file's safe identifier as a prefix.
CLASHING_SYMS = [
    'kUnityQ12',
    'kMaxU12',
    'U12ToQ12',
    'kMaxU16',
    'kFiveVoltInputCode',
    'kBasePhaseInc10Hz',
    'kMaxPhaseInc10kHz',
    'kSemitoneQ16',
    'kExp2Q16',
    'kPitchCVScaleNumerator',
    'kPitchCVScaleDenominator',
    'ClampU12',
    'ClampBipolarFiveVoltInput',
    'MapBipolarFiveVoltInputToU12',
    'ApplyU12Attenuation',
    'MapPositiveFiveVoltInputToQ12',
    'MapPitchCVInputToPitchCodeDelta',
    'MIDIMessageLength',
    'kMaxMIDIPollBytes',
]

def post_process(src_content, src_rel):
    # Knots uses vreg_set_voltage and set_sys_clock_khz in main() — stub them out
    src_content = src_content.replace('vreg_set_voltage(VREG_VOLTAGE_1_15);', '')
    src_content = src_content.replace('set_sys_clock_khz(200000, true);', '')

    # Skip main.cpp — it keeps the canonical names
    if src_rel == 'main.cpp':
        return src_content

    # Derive a short safe prefix from the file path
    safe_id = re.sub(r'[^a-zA-Z0-9]', '_', src_rel.replace('.cpp', '').replace('.c', ''))
    safe_id = re.sub(r'_+', '_', safe_id).strip('_')

    # Rename each clashing symbol to file_safe_id__OriginalName
    for sym in CLASHING_SYMS:
        # Check if the symbol is actually defined in this file. If not, don't rename it.
        def_pattern = r'(?:constexpr|static|#define|inline|uint32_t|int32_t|uint16_t|int16_t|int|uint)\s+' + re.escape(sym) + r'\b'
        if not re.search(def_pattern, src_content):
            continue
        # Use word-boundary match to avoid partial replacements
        src_content = re.sub(r'\b' + re.escape(sym) + r'\b', f'{safe_id}__{sym}', src_content)

    return src_content

def get_extra_include_dirs(card_dir_abs):
    """
    Knots uses '#include "knots/src/core/..."' so the parent of knots/src
    must be on the include path. card_dir_abs is .../knots/src, so
    we need .../knots and .../src as additional includes.
    """
    knots_dir = os.path.dirname(card_dir_abs)   # .../knots
    src_dir = os.path.dirname(knots_dir)         # .../src
    return [knots_dir, src_dir]
