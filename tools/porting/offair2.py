import re

def post_process(src_content, src_rel):
    if src_rel == "main.cpp":
        # Add mocks for DEBUG_1 and DEBUG_2
        src_content = "#define DEBUG_1 20\n#define DEBUG_2 21\n" + src_content
        
        # Replace OnRFPWMWrap body (whitespace-tolerant)
        src_content = re.sub(
            r'static\s+void\s+__not_in_flash_func\(OnRFPWMWrap\)\(\)\s*\{\s*static\s+int32_t\s+amError[^}]*\}',
            'static void __not_in_flash_func(OnRFPWMWrap)() {}',
            src_content,
            flags=re.DOTALL
        )
        
        # Replace SetupRFPWM body (whitespace-tolerant)
        src_content = re.sub(
            r'static\s+void\s+SetupRFPWM\(\)\s*\{\s*gpio_set_function[^}]*\}',
            'static void SetupRFPWM() {}',
            src_content,
            flags=re.DOTALL
        )
        
        # Replace main body (whitespace-tolerant, handles comments and injected return 0)
        src_content = re.sub(
            r'int\s+main\(\)\s*\{\s*.*?set_sys_clock_khz.*?while\s*\(true\)\s*__wfi\(\);\s*.*?\}',
            'int main() {\n\tcard.Init();\n\tcard.Run();\n\treturn 0;\n}',
            src_content,
            flags=re.DOTALL
        )
        
    return src_content
