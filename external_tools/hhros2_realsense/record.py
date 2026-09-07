#!/usr/bin/env python3
"""
Stable entrypoint for RealSense photo/video capture.

This wrapper reuses the implementation in record_preview_log.py because that
version has already been validated on this machine to exit cleanly after
recording. Keep record.py as the user-facing command.
"""

from record_preview_log import main


if __name__ == "__main__":
    main()
