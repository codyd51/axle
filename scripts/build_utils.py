#!/usr/local/bin/python3
import io
import os
import platform
import selectors
import shutil
import subprocess
import sysconfig
import urllib
import requests
import sys
from pathlib import Path
from typing import Dict, List, Optional


def second_file_is_older(file1: Path, file2: Path) -> bool:
    return os.stat(file1.as_posix()).st_mtime - os.stat(file2.as_posix()).st_mtime > 1


def copied_file_is_outdated(source_path: Path, copied_path: Path) -> bool:
    return second_file_is_older(source_path, copied_path)


def is_arm64_process() -> bool:
    # Are we currently running in 'native' arm64 mode?
    return platform.machine() == 'arm64'


def is_on_macos() -> bool:
    return 'macosx' in sysconfig.get_platform()


def run_and_check(cmd_list: List[str], cwd: Path = None, env_additions: Optional[Dict[str, str]] = None) -> None:
    print(" ".join(cmd_list), cwd)
    env = os.environ.copy()

    # PT: Ensure the M1 Homebrew prefix is always in PATH
    # TODO(PT): Conditionally enable this on M1 hosts?
    # And ensure we do this before we add in the environment additions from the caller, so those always have
    # highest precedence.
    env["PATH"] = f"/opt/homebrew/bin:{env['PATH']}"

    if env_additions:
        for k, v in env_additions.items():
            env[k] = v

    status = subprocess.run(cmd_list, cwd=cwd.as_posix() if cwd else None, env=env)
    if status.returncode != 0:
        raise RuntimeError(f'Running "{" ".join(cmd_list)}" failed with exit code {status.returncode}')


def run_and_capture_output_and_check(cmd_list: List[str], cwd: Path = None) -> str:
    """Beware this will strip ASCII escape codes, so you'll lose colors."""
    # https://gist.github.com/nawatts/e2cdca610463200c12eac2a14efc0bfb
    # Start subprocess
    # bufsize = 1 means output is line buffered
    # universal_newlines = True is required for line buffering
    process = subprocess.Popen(
        cmd_list,
        cwd=cwd.as_posix() if cwd else None,
        bufsize=1,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        universal_newlines=True,
    )

    # Create callback function for process output
    buf = io.StringIO()

    def handle_output(stream, mask):
        # Because the process' output is line buffered, there's only ever one
        # line to read when this function is called
        line = stream.readline()
        buf.write(line)
        sys.stdout.write(line)

    # Register callback for an "available for read" event from subprocess' stdout stream
    selector = selectors.DefaultSelector()
    selector.register(process.stdout, selectors.EVENT_READ, handle_output)

    # Loop until subprocess is terminated
    while process.poll() is None:
        # Wait for events and handle them with their registered callbacks
        events = selector.select()
        for key, mask in events:
            callback = key.data
            callback(key.fileobj, mask)

    # Get process return code
    return_code = process.wait()
    selector.close()

    # Store buffered output
    output = buf.getvalue()
    buf.close()

    if return_code != 0:
        raise RuntimeError(f'Running "{" ".join(cmd_list)}" failed with exit code {return_code}')

    return output


def sizeof_fmt(num, suffix="B"):
    for unit in ["", "Ki", "Mi", "Gi", "Ti", "Pi", "Ei", "Zi"]:
        if abs(num) < 1024.0:
            return f"{num:3.1f}{unit}{suffix}"
        num /= 1024.0
    return f"{num:.1f}Yi{suffix}"


def download_file(directory: Path, url: str) -> Path:
    cache_dir = Path(__file__).parent / "caches"
    cache_dir.mkdir(exist_ok=True)

    local_filename = url.split("/")[-1]
    download_path = directory / local_filename
    cache_file = cache_dir / local_filename

    if cache_file.exists():
        print(f'Providing {local_filename} from cache...')
        shutil.copy(cache_file, download_path)
        return download_path

    print(f"Downloading {url} to {download_path}...")

    with open(download_path.as_posix(), "wb") as f:
        if url.startswith('ftp://'):
            urllib.request.urlretrieve(url, download_path.as_posix())
        else:
            # NOTE the stream=True parameter below
            with requests.get(url, stream=True) as r:
                r.raise_for_status()
                for chunk in r.iter_content(chunk_size=8192):
                    f.write(chunk)
    print(f'File size: {sizeof_fmt(download_path.stat().st_size)}')

    # Copy downloaded file to our cache
    shutil.copy(download_path, cache_file)

    return download_path


def download_and_unpack_archive(parent: Path, url: str) -> Path:
    if not url.endswith(".tar.gz"):
        raise ValueError(f"Expected URL pointing to a .tar.gz, got: {url}")

    archive = download_file(parent, url)
    folder_name = url.split("/")[-1].removesuffix(".tar.gz")
    print(f"Got {folder_name} archive at {archive}")

    shutil.unpack_archive(archive.as_posix(), parent.as_posix())
    folder_path = parent / folder_name

    if not folder_path.exists():
        raise ValueError(f"Expected directory was not extracted: {folder_path}")

    print(f"Extracted {folder_name} to {folder_path}")
    return folder_path
