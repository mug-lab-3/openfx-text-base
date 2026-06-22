#!/bin/bash
cd "$(dirname "$0")"

# Add python user bin directory and local bin to PATH in case they are not in the current shell's PATH
PYTHON_USER_BIN=$(python3 -c "import site; print(site.getuserbase() + '/bin')" 2>/dev/null || echo "")
export PATH="$PYTHON_USER_BIN:$HOME/.local/bin:/usr/local/bin:$PATH"

uv run python viewer.py
