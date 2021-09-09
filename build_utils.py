#!/usr/local/bin/python3
import io
import sys
import selectors
import subprocess
from pathlib import Path
from typing import List


def run_and_check(cmd_list: List[str], cwd: Path) -> None:
    status = subprocess.run(cmd_list, cwd=cwd.as_posix())
    if status.returncode != 0:
        raise RuntimeError(f'Running "{" ".join(cmd_list)}" failed with exit code {status.returncode}')


def run_and_capture_output_and_check(cmd_list: List[str], cwd: Path) -> None:
    """Beware this will strip ASCII escape codes, so you'll lose colors.
    """
    # https://gist.github.com/nawatts/e2cdca610463200c12eac2a14efc0bfb
    # Start subprocess
    # bufsize = 1 means output is line buffered
    # universal_newlines = True is required for line buffering
    process = subprocess.Popen(
        cmd_list,
        cwd=cwd.as_posix(),
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
