import os

def get_extra_include_dirs(card_dir_abs):
    return [
        os.path.join(card_dir_abs, "dsp")
    ]
