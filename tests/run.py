"""Run production storage/encoding tests with a host GCC toolchain."""
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parent
compiler = shutil.which(os.environ.get('CC', 'gcc'))
if not compiler:
    raise SystemExit('GCC required; set CC to the host compiler executable.')
with tempfile.TemporaryDirectory(prefix='nfc_qr_tests_') as directory:
    binary = Path(directory) / ('storage_test.exe' if os.name == 'nt' else 'storage_test')
    subprocess.run([compiler, '-std=c11', '-Wall', '-Wextra', '-Werror',
                    '-I' + str(root / 'stubs'), str(root / 'storage_test.c'),
                    '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
