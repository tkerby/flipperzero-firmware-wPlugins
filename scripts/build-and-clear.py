import os
import glob
import lief
import pathlib

OBJCOPY_PATH = (
    "C:/Users/denr01/.ufbt/toolchain/x86_64-windows/bin/arm-none-eabi-objcopy.exe"
)

FAP_LOCATION_ON_FLIPPER = "/ext/apps/Sub-GHz/chief_cooker.fap"

UFBT_PATH = pathlib.Path.home() / ".ufbt"

fapDir = pathlib.Path(__file__).parent.parent.resolve() / "dist/*.fap"
fapFile = glob.glob(str(fapDir))[0]
print("Using FAP file: " + fapFile)


def clearSections():
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


os.system("ufbt build")
clearSections()
os.system(
    "%s/toolchain/current/python/python %s/current/scripts/runfap.py -p auto -s %s -t %s"
    % (UFBT_PATH, UFBT_PATH, fapFile, FAP_LOCATION_ON_FLIPPER)
)
