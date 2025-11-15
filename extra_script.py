# extra_script.py
import subprocess
Import("env")

# Get the short Git revision
try:
    ret = subprocess.run(
        ["git", "rev-parse", "--short", "HEAD"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=True,
        text=True
    )
    git_rev = ret.stdout.strip()
except Exception as e:
    print(f"Error getting Git revision: {e}")
    git_rev = "unknown"


# Append the Git revision as a build flag
# This will add: -D APP_BUILD_NUMBER='"<git_rev>"'
# The quotes are escaped for the C/C++ preprocessor
env.Append(
    BUILD_FLAGS=[
        f'-D APP_BUILD_NUMBER="\\"{git_rev}\\""'
    ]
)

print(f"Firmware build number: {git_rev}")
