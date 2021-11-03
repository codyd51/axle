import subprocess
import argparse
import sys


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--program', action="store")
    parser.add_argument("--kernel", action="store_true")
    parser.set_defaults(kernel=False)
    args = parser.parse_args()

    if args.kernel:
        path = './isodir/boot/axle.bin'
    else:
        path = f'./initrd/{args.program}'


    subprocess.run(f"hopperv4 -e {path}", shell=True)


if __name__ == "__main__":
    main()
