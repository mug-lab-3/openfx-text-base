#!/bin/bash
set -e

INSTALL_DIR="/Library/OFX/Plugins"
BUNDLE_NAME="OfxBase.ofx.bundle"

# When double-clicked, the working directory is not set to the script location.
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"

# Find the bundle next to this script
BUNDLE_PATH="$SCRIPT_DIR/$BUNDLE_NAME"
if [ ! -d "$BUNDLE_PATH" ]; then
    echo "Error: $BUNDLE_NAME not found next to this installer."
    echo "Expected location: $BUNDLE_PATH"
    read -p "Press Enter to exit..."
    exit 1
fi

echo "Installing $BUNDLE_NAME to $INSTALL_DIR ..."
echo "(You will be prompted for your password)"
echo ""

sudo mkdir -p "$INSTALL_DIR"

if [ -d "$INSTALL_DIR/$BUNDLE_NAME" ]; then
    sudo rm -rf "$INSTALL_DIR/$BUNDLE_NAME"
fi

sudo cp -R "$BUNDLE_PATH" "$INSTALL_DIR/"

echo "Removing quarantine attributes (Gatekeeper bypass) ..."
sudo xattr -cr "$INSTALL_DIR/$BUNDLE_NAME"

echo ""
echo "Installation complete."
echo "  Location: $INSTALL_DIR/$BUNDLE_NAME"
echo ""
echo "Please launch (or restart) DaVinci Resolve and verify the plugin is loaded."
echo ""
read -p "Press Enter to exit..."
osascript -e 'tell application "Terminal" to close front window' &
