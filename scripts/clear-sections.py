import os
import glob
import lief
import pathlib

OBJCOPY_PATH = (
    "C:/Users/denr01/.ufbt/toolchain/x86_64-windows/bin/arm-none-eabi-objcopy.exe"
)

fapDir = pathlib.Path(__file__).parent.parent.resolve() / "dist/*.fap"
fapFile = glob.glob(str(fapDir))[0]
print("Using FAP file: " + fapFile)

binary = lief.parse(fapFile)

for i, sect in enumerate(binary.sections):
    if sect.name.find("_Z") >= 0:
        newSectName = "s%d" % i
        cmd = '%s "%s" --rename-section %s=%s' % (
            OBJCOPY_PATH,
            fapFile,
            sect.name,
            newSectName,
        )
        print("Renaming to %s from %s" % (newSectName, sect.name))
        os.system(cmd)
