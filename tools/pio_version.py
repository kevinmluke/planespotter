# PlatformIO pre-script: injects FW_VERSION from the VERSION file so the
# firmware, the signed manifest and the OTA check all agree on one number.
Import("env")  # noqa: F821
import os

root = env["PROJECT_DIR"]  # noqa: F821
with open(os.path.join(root, "VERSION")) as f:
    version = f.read().strip()

env.Append(CPPDEFINES=[("FW_VERSION", '\\"%s\\"' % version)])  # noqa: F821
print("PlaneSpotter firmware version", version)
