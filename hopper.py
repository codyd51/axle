import subprocess
import sys


def main():
    subprocess.run(f"hopperv4 -e ./initrd/{sys.argv[1]}", shell=True)


if __name__ == "__main__":
    main()
