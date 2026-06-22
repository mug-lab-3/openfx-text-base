#!/bin/bash
set -e

# Ensure script runs from its own directory
cd "$(dirname "$0")"

# Add python user bin directory and local bin to PATH in case they are not in the current shell's PATH
PYTHON_USER_BIN=$(python3 -c "import site; print(site.getuserbase() + '/bin')" 2>/dev/null || echo "")
export PATH="$PYTHON_USER_BIN:$HOME/.local/bin:/usr/local/bin:$PATH"

# Check if uv is installed, if not try to install it
if ! command -v uv &> /dev/null; then
    echo "uv not found. Trying to install uv via pip3..."
    python3 -m pip install --user uv || python3 -m pip install uv || pip3 install uv
fi

echo "Syncing dependencies via uv..."
uv sync

echo ""
echo "Setup complete. Run: ./run.sh"
