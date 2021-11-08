#!/usr/local/bin/python3
import io
import os
import selectors
import shutil
import subprocess
import sys
from pathlib import Path
from typing import Dict, List, Optional

import requests


def copied_file_is_outdated(source_path: Path, copied_path: Path) -> bool:
    return os.stat(source_path.as_posix()).st_mtime - os.stat(copied_path.as_posix()).st_mtime > 1


def run_and_check(cmd_list: List[str], cwd: Path = None, env_additions: Optional[Dict[str, str]] = None) -> None:
    print(" ".join(cmd_list))
    env = {}
    if env_additions:
        env = os.environ.copy()
        for k, v in env_additions.items():
            env[k] = v

    status = subprocess.run(cmd_list, cwd=cwd.as_posix() if cwd else None, env=env if env_additions else None)
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


def download_file(directory: Path, url: str) -> Path:
    local_filename = url.split("/")[-1]
    download_path = directory / local_filename
    print(f"Downloading {url} to {download_path}...")
    # NOTE the stream=True parameter below
    with requests.get(url, stream=True) as r:
        r.raise_for_status()
        with open(download_path.as_posix(), "wb") as f:
            for chunk in r.iter_content(chunk_size=8192):
                f.write(chunk)
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
