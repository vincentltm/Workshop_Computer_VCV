import os
import re

# SCRIPT_DIR is tools/porting/
SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
VCV_PROJECT_DIR = os.path.abspath(os.path.join(SCRIPT_DIR, "..", ".."))
FLUX_INC_DIR = os.path.join(VCV_PROJECT_DIR, "deps", "external", "multifx", "50_flux", "lib", "pipicofx", "inc")

struct_map = None

def parse_statements_for_fields_and_types(fields_block):
    fields = []
    types = {}
    for statement in fields_block.split(';'):
        statement = statement.strip()
        if not statement or statement.startswith('#'):
            continue
        parts = statement.split(',')
        if not parts:
            continue
        first_part = parts[0].strip()
        first_var_match = re.search(r'\b([A-Za-z0-9_]+)\s*(?:\[[^\]]*\])*\s*$', first_part)
        if not first_var_match:
            continue
        first_var = first_var_match.group(1)
        type_str = first_part[:first_var_match.start()].strip()
        type_clean = re.sub(r'\b(const|volatile|struct|unsigned|signed|static)\b', '', type_str).strip()
        
        fields.append(first_var)
        types[first_var] = type_clean
        
        for part in parts[1:]:
            part = part.strip()
            var_match = re.search(r'\b([A-Za-z0-9_]+)\s*(?:\[[^\]]*\])*\s*$', part)
            if var_match:
                var_name = var_match.group(1)
                fields.append(var_name)
                types[var_name] = type_clean
    return fields, types

def parse_all_structs(inc_dir):
    s_map = {}
    if not os.path.isdir(inc_dir):
        return s_map
    for root, dirs, files in os.walk(inc_dir):
        for file in files:
            if file.endswith('.h'):
                path = os.path.join(root, file)
                try:
                    with open(path, 'r', encoding='utf-8', errors='ignore') as f:
                        content = f.read()
                    content = re.sub(r'/\*[\s\S]*?\*/', '', content)
                    content = re.sub(r'//.*', '', content)
                    for m in re.finditer(r'typedef struct\s*\{([\s\S]*?)\}\s*([A-Za-z0-9_]+)\s*;', content):
                        fields_block = m.group(1)
                        struct_name = m.group(2)
                        fields, types = parse_statements_for_fields_and_types(fields_block)
                        if struct_name not in s_map or len(fields) > len(s_map[struct_name]["fields"]):
                            s_map[struct_name] = { "fields": fields, "types": types }
                    for m in re.finditer(r'struct\s+([A-Za-z0-9_]+)\s*\{([\s\S]*?)\}\s*;', content):
                        struct_name = m.group(1)
                        fields_block = m.group(2)
                        fields, types = parse_statements_for_fields_and_types(fields_block)
                        if struct_name not in s_map or len(fields) > len(s_map[struct_name]["fields"]):
                            s_map[struct_name] = { "fields": fields, "types": types }
                except Exception as e:
                    print(f"Warning: Error parsing header {path}: {e}")
    return s_map

def parse_structs_from_text(content, s_map):
    content = re.sub(r'/\*[\s\S]*?\*/', '', content)
    content = re.sub(r'//.*', '', content)
    for m in re.finditer(r'typedef struct\s*\{([\s\S]*?)\}\s*([A-Za-z0-9_]+)\s*;', content):
        fields_block = m.group(1)
        struct_name = m.group(2)
        fields, types = parse_statements_for_fields_and_types(fields_block)
        if struct_name not in s_map or len(fields) > len(s_map[struct_name]["fields"]):
            s_map[struct_name] = { "fields": fields, "types": types }
    for m in re.finditer(r'struct\s+([A-Za-z0-9_]+)\s*\{([\s\S]*?)\}\s*;', content):
        struct_name = m.group(1)
        fields_block = m.group(2)
        fields, types = parse_statements_for_fields_and_types(fields_block)
        if struct_name not in s_map or len(fields) > len(s_map[struct_name]["fields"]):
            s_map[struct_name] = { "fields": fields, "types": types }

def get_struct_map():
    global struct_map
    if struct_map is None:
        struct_map = parse_all_structs(FLUX_INC_DIR)
    return struct_map

def parse_braces(content, start_pos):
    depth = 0
    pos = start_pos
    while pos < len(content):
        c = content[pos]
        if c == '{':
            depth += 1
        elif c == '}':
            depth -= 1
            if depth == 0:
                return pos
        pos += 1
    return -1

def is_designated_struct_initializer(inner_text):
    inner_clean = re.sub(r'/\*[\s\S]*?\*/', '', inner_text)
    inner_clean = re.sub(r'//.*', '', inner_clean)
    depth = 0
    has_dot = False
    has_equal = False
    pos = 0
    n = len(inner_clean)
    while pos < n:
        c = inner_clean[pos]
        if c in ('{', '('):
            depth += 1
            pos += 1
        elif c in ('}', ')'):
            depth -= 1
            pos += 1
        elif depth == 0 and c == '.':
            if pos + 1 < n and inner_clean[pos+1].isalpha():
                has_dot = True
            pos += 1
        elif depth == 0 and c == '=':
            if has_dot:
                has_equal = True
            pos += 1
        else:
            pos += 1
    return has_dot and has_equal

def parse_array_initializer(inner_text):
    elements = []
    pos = 0
    n = len(inner_text)
    current_element = []
    depth = 0
    while pos < n:
        c = inner_text[pos]
        if c == '{':
            depth += 1
            current_element.append(c)
            pos += 1
        elif c == '}':
            depth -= 1
            current_element.append(c)
            pos += 1
            if depth == 0:
                elements.append("".join(current_element).strip())
                current_element = []
        elif depth == 0 and c == ',':
            elem_str = "".join(current_element).strip()
            if elem_str:
                elements.append(elem_str)
            current_element = []
            pos += 1
        else:
            current_element.append(c)
            pos += 1
    elem_str = "".join(current_element).strip()
    if elem_str:
        elements.append(elem_str)
    return elements

def is_array_of_designated_structs(inner_text):
    elements = parse_array_initializer(inner_text)
    if not elements:
        return False
    for e in elements:
        if e.startswith('{') and e.endswith('}'):
            if is_designated_struct_initializer(e[1:-1]):
                return True
    return False

def is_array_initializer(inner_text):
    return not is_designated_struct_initializer(inner_text)

def parse_assignments(inside_text):
    inside_text = re.sub(r'/\*[\s\S]*?\*/', '', inside_text)
    inside_text = re.sub(r'//.*', '', inside_text)
    
    assignments = {}
    depth = 0
    current_field = None
    current_val = []
    pos = 0
    n = len(inside_text)
    while pos < n:
        c = inside_text[pos]
        if c in ('{', '('):
            depth += 1
            if current_field:
                current_val.append(c)
            pos += 1
        elif c in ('}', ')'):
            depth -= 1
            if current_field:
                current_val.append(c)
            pos += 1
        elif depth == 0 and c == '.':
            if current_field:
                val_str = "".join(current_val).strip().rstrip(',')
                if val_str.startswith('{') and val_str.endswith('}'):
                    inner = val_str[1:-1].strip()
                    if is_designated_struct_initializer(inner):
                        assignments[current_field] = parse_assignments(inner)
                    elif is_array_of_designated_structs(inner) and current_field != 'parameters':
                        assignments[current_field] = [parse_assignments(e[1:-1]) for e in parse_array_initializer(inner)]
                    else:
                        assignments[current_field] = val_str
                else:
                    assignments[current_field] = val_str
                current_val = []
            pos += 1
            field_match = re.match(r'^([A-Za-z0-9_.]+)', inside_text[pos:])
            if field_match:
                current_field = field_match.group(1)
                pos += len(current_field)
                while pos < n and inside_text[pos] in (' ', '\t', '\n', '\r', '='):
                    pos += 1
            else:
                current_field = None
        elif depth == 0 and c == ',':
            if current_field:
                val_str = "".join(current_val).strip()
                if val_str.startswith('{') and val_str.endswith('}'):
                    inner = val_str[1:-1].strip()
                    if is_designated_struct_initializer(inner):
                        assignments[current_field] = parse_assignments(inner)
                    elif is_array_of_designated_structs(inner) and current_field != 'parameters':
                        assignments[current_field] = [parse_assignments(e[1:-1]) for e in parse_array_initializer(inner)]
                    else:
                        assignments[current_field] = val_str
                else:
                    assignments[current_field] = val_str
                current_field = None
                current_val = []
            pos += 1
        else:
            if current_field:
                current_val.append(c)
            pos += 1
    if current_field:
        val_str = "".join(current_val).strip().rstrip(',')
        if val_str.startswith('{') and val_str.endswith('}'):
            inner = val_str[1:-1].strip()
            if is_designated_struct_initializer(inner):
                assignments[current_field] = parse_assignments(inner)
            elif is_array_of_designated_structs(inner) and current_field != 'parameters':
                assignments[current_field] = [parse_assignments(e[1:-1]) for e in parse_array_initializer(inner)]
            else:
                assignments[current_field] = val_str
        else:
            assignments[current_field] = val_str
    return assignments



def parse_parameter_array(params_text):
    params_text = params_text.strip()
    if params_text.startswith('{') and params_text.endswith('}'):
        params_text = params_text[1:-1].strip()
    param_blocks = []
    pos = 0
    n = len(params_text)
    while pos < n:
        if params_text[pos] == '{':
            end = parse_braces(params_text, pos)
            if end != -1:
                param_blocks.append(params_text[pos+1:end])
                pos = end + 1
            else:
                pos += 1
        else:
            pos += 1
    return param_blocks

def fix_parameters_array(params_text):
    param_blocks = parse_parameter_array(params_text)
    formatted_params = []
    for block in param_blocks:
        fields = parse_assignments(block)
        name = fields.get('name', '""')
        control = fields.get('control', '255')
        rawValue = fields.get('rawValue', '0')
        increment = fields.get('increment', '1')
        getParameterValue = fields.get('getParameterValue', '0')
        getParameterDisplay = fields.get('getParameterDisplay', '0')
        setParameter = fields.get('setParameter', '0')
        formatted_params.append(f"""{{
            .name = {name},
            .control = {control},
            .rawValue = {rawValue},
            .increment = {increment},
            .getParameterValue = {getParameterValue},
            .getParameterDisplay = {getParameterDisplay},
            .setParameter = {setParameter}
        }}""")
    while len(formatted_params) < 8:
        formatted_params.append("""{
            .name = "",
            .control = 255,
            .rawValue = 0,
            .increment = 1,
            .getParameterValue = 0,
            .getParameterDisplay = 0,
            .setParameter = 0
        }""")
    return "{\n        " + ",\n        ".join(formatted_params) + "\n    }"

def group_nested_fields(assignments):
    grouped = {}
    for key, val in assignments.items():
        if '.' in key:
            parts = key.split('.')
            parent = parts[0]
            child = ".".join(parts[1:])
            if parent not in grouped:
                grouped[parent] = {}
            if isinstance(grouped[parent], dict):
                grouped[parent][child] = val
        else:
            grouped[key] = val
    res = {}
    for key, val in grouped.items():
        if isinstance(val, dict):
            res[key] = group_nested_fields(val)
        else:
            res[key] = val
    return res

def format_nested_dict(d, type_name, local_struct_map, indent=4):
    keys = list(d.keys())
    if type_name in local_struct_map:
        field_order = local_struct_map[type_name]["fields"]
        keys.sort(key=lambda k: field_order.index(k) if k in field_order else 999)
    lines = []
    for key in keys:
        val = d[key]
        member_type = None
        if type_name in local_struct_map:
            member_type = local_struct_map[type_name]["types"].get(key)
        if isinstance(val, dict):
            sub_str = format_nested_dict(val, member_type, local_struct_map, indent + 4)
            lines.append(f"{' ' * indent}.{key} = {sub_str}")
        elif isinstance(val, list):
            formatted_elements = []
            for elem in val:
                if isinstance(elem, dict):
                    formatted_elements.append(format_nested_dict(elem, member_type, local_struct_map, indent + 8))
                else:
                    formatted_elements.append(str(elem))
            lines.append(f"{' ' * indent}.{key} = {{\n" + ",\n".join([f"{' ' * (indent+4)}{elem}" for elem in formatted_elements]) + "\n" + ' ' * indent + "}")
        else:
            lines.append(f"{' ' * indent}.{key} = {val}")
    return "{\n" + ",\n".join(lines) + "\n" + " " * (indent - 4) + "}"

def rewrite_inside(type_name, inside, local_struct_map):
    assignments = parse_assignments(inside)
    if type_name == "FxProgramType":
        if 'parameters' in assignments:
            assignments['parameters'] = fix_parameters_array(assignments['parameters'])
        return format_nested_dict(assignments, type_name, local_struct_map, indent=4)
    else:
        grouped = group_nested_fields(assignments)
        return format_nested_dict(grouped, type_name, local_struct_map, indent=4)

def rewrite_initializers(content, local_struct_map):
    pattern = re.compile(r'\b([A-Za-z0-9_]+Type)\s+([A-Za-z0-9_]+)\s*=\s*\{')
    offset = 0
    while True:
        match = pattern.search(content, offset)
        if not match:
            break
        type_name = match.group(1)
        var_name = match.group(2)
        start_brace = match.end() - 1
        end_brace = parse_braces(content, start_brace)
        if end_brace == -1:
            offset = match.end()
            continue
        inside = content[start_brace+1:end_brace]
        new_inside = rewrite_inside(type_name, inside, local_struct_map)
        content = content[:start_brace] + new_inside + content[end_brace+1:]
        offset = start_brace + len(new_inside)
    return content

def post_process(src_content, src_rel):
    # Original replacements
    src_content = src_content.replace(
        'bool SynthCore_GetSample(int16_t *outL, int16_t *outR) {',
        '''bool SynthCore_GetSample(int16_t *outL, int16_t *outR) {
  int count = (rbHead - rbTail + SYNTH_RING_BUF_SIZE) % SYNTH_RING_BUF_SIZE;
  if (count < 64) {
      g_synth_need_render.store(true);
      g_synth_cv.notify_one();
  }'''
    )
    src_content = src_content.replace(
        'void __attribute__((used)) GetLiveState(uint8_t *outBlob) {',
        'public:\n  void __attribute__((used)) GetLiveState(uint8_t *outBlob) {'
    )
    src_content = re.sub(
        r'class\s+MultiFx_Computer\s*\{[^}]*GetLiveState[^}]*\};',
        '/* stripped dummy class MultiFx_Computer */',
        src_content
    )
    if "exptable.c" in src_rel:
        src_content = src_content.replace('exptable', 'exptable_impl')
    if "logtable.c" in src_rel:
        src_content = src_content.replace('logtable', 'logtable_impl')
    src_content = src_content.replace(
        'restore_interrupts(interrupts);\n  SynthCore_Start();\n}',
        'restore_interrupts(interrupts);\n  // SynthCore_Start() suppressed in VCV: core1 never stops during flash save\n}'
    )
    src_content = src_content.replace(
        '    if ((uintptr_t)ptr > ((XIP_BASE + 0x00180000) + 16 * 1024 * 1024))\n      break;',
        '    if ((uintptr_t)ptr >= (XIP_BASE + PICO_FLASH_SIZE_BYTES))\n      break;'
    )
    src_content = src_content.replace(
        'numSamples > (16 * 1024 * 1024)',
        'numSamples > PICO_FLASH_SIZE_BYTES'
    )

    # Reorder and fix designated initializers
    if "fxProgram" in src_rel or "fxPrograms.c" in src_rel:
        global_map = get_struct_map()
        local_map = dict(global_map)
        parse_structs_from_text(src_content, local_map)
        src_content = rewrite_initializers(src_content, local_map)

    return src_content

def get_extra_definitions():
    return """    extern const int16_t exptable_impl[];
    extern const int16_t logtable_impl[];
    const int16_t * exptable = exptable_impl;
    const int16_t * logtable = logtable_impl;

    struct BootRomInfoType;
    float fsqrt(float a) { return ::sqrtf(a); }
    float fcos(float a) { return ::cosf(a); }
    float fsin(float a) { return ::sinf(a); }
    float ftan(float a) { return ::tanf(a); }
    float fexp(float a) { return ::expf(a); }
    float fln(float a) { return ::logf(a); }
    int32_t float2int(float a) { return (int32_t)a; }
    float int2float(int32_t a) { return (float)a; }
    void initFloatFunctions() {}
    void * getRomData(char c1,char c2) { return nullptr; }
    void getBootRomInfo(BootRomInfoType** bootRomInfo) { if (bootRomInfo) *bootRomInfo = nullptr; }
    char * getCopyright() { return (char*)"Mock"; }
    void reset_usb_boot(uint32_t gpio_activity_mask, uint32_t disable_interface_mask) {}
    void flash_range_erase(uint32_t addr, uint32_t count, uint32_t block_size, uint8_t block_cmd) {}
    void flash_range_program(uint32_t addr, const uint8_t *data, uint32_t count) {}
    float __aeabi_fadd(float a,float b) { return a + b; }
    float __aeabi_fsub(float a,float b) { return a - b; }
    float __aeabi_frsub(float a,float b) { return b - a; }
    float __aeabi_fmul(float a,float b) { return a * b; }
    float __aeabi_fdiv(float a,float b) { return a / b; }
"""
