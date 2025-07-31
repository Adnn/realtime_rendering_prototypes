#!python

# Taking an input JSON file with categories (key) and array of images (values),
# apply nvtt_export to each files under each category.
# Parameters to nvtt_export are defined by category in `params_by_key`.
# Note: The loader code produce a categorized output in `sorted_textures.json`

import json
import os
import subprocess
import sys

# Parameters for each key
params_by_key = {
    "diffuse": ["--format", "bc7", "--dx10", "--quality", "normal", "--mips", "--mip-filter", "box", "--save-flip-y", "--export-transfer-function", "srgb", "--zcmp", "5"],
    "normal": ["--format", "bc5", "--dx10", "--quality", "normal", "--mips", "--mip-filter", "box", "--no-mip-gamma-correct", "--save-flip-y", "--export-transfer-function", "linear", "--zcmp", "5"],
    "mrao": ["--format", "bc7", "--dx10", "--quality", "normal", "--mips", "--mip-filter", "box", "--save-flip-y", "--export-transfer-function", "linear", "--zcmp", "5"],
}

# Parse JSON
data_path = sys.argv[1]
data = json.load(open(data_path))
prefix = os.path.dirname(os.path.abspath(data_path))

for key, values in data.items():
    params = params_by_key.get(key, [])
    for val in values:
        cmd = ["nvtt_export"] + params + [os.path.join(prefix, val)]
        subprocess.run(cmd)
