#!/usr/local/bin/python3
from build_utils import run_and_check


def main() -> None:
    run_and_check(['qemu-img', 'create', '-f', 'raw', 'axle-hdd.img', '16M'])


if __name__ == '__main__':
    main()
