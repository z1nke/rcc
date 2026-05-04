import lit.formats

import os
import shlex

config.name = "rcc"
config.test_format = lit.formats.ShTest(True)

config.suffixes = ['.c']
config.excludes = ['Inputs', 'CMakeLists.txt', 'README.txt', 'README.md', 
  "LICENSE", "LICENSE.txt"]

config.test_source_root = os.path.dirname(__file__)
config.test_exec_root = os.path.join(config.rcc_tools_binary_dir, 'test')

python_exec = shlex.quote(config.python_executable)
check_rcc_run = os.path.join(
    config.test_source_root, "check_rcc_run.py")
config.substitutions.append(
    ('%check_rcc_run', f'{python_exec} {check_rcc_run}') )

check_rcc_pp_run = os.path.join(
    config.test_source_root, "check_rcc_pp_run.py")
config.substitutions.append(
    ('%check_rcc_pp_run',
     f'{python_exec} {check_rcc_pp_run}') )