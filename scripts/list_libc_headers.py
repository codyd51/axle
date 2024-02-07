from pathlib import Path


def list_libc_headers() -> None:
    sysroot_path = Path(__file__).parents[1] / 'axle-sysroot' / 'usr' / 'include'
    for p in sorted(sysroot_path.rglob("*")):
        if p.is_dir():
            continue
        relative_path = p.relative_to(sysroot_path)
        print(f'        "{relative_path}",')


if __name__ == '__main__':
    list_libc_headers()
    