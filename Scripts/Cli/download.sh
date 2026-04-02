#!/bin/bash
while [[ $# -gt 0 ]]; do
    case "$1" in
        --plugin-dir) PLUGIN_DIR="$2"; shift 2 ;;
        *) echo "Unknown argument: $1"; exit 1 ;;
    esac
done

echo "[CastToCloud] CLI download script started"

VERSION="latest"
if [ -f "$PLUGIN_DIR/Scripts/Cli/version.txt" ]; then
    PINNED_VERSION="$(tr -d '[:space:]' < "$PLUGIN_DIR/Scripts/Cli/version.txt")"
    if [ -n "$PINNED_VERSION" ]; then VERSION="$PINNED_VERSION"; fi
fi

download() {
    local PLATFORM="$1" SUBDIR="$2" FILENAME="$3"
    local CLI_PATH="$PLUGIN_DIR/Binaries/$SUBDIR/$FILENAME"
    local VERSION_PATH="$PLUGIN_DIR/Binaries/$SUBDIR/casttocloud-cli.version"

    if [ -f "$CLI_PATH" ] && [ -f "$VERSION_PATH" ] && [ "$(tr -d '[:space:]' < "$VERSION_PATH")" = "$VERSION" ]; then
        echo "[CastToCloud] $PLATFORM CLI $VERSION up to date, skipping"
        return
    fi

    mkdir -p "$(dirname "$CLI_PATH")"
    echo "[CastToCloud] Downloading $PLATFORM CLI $VERSION -> $CLI_PATH"

    if curl -fsSL -L -o "$CLI_PATH" "https://api.casttocloud.com/cli/download?platform=$PLATFORM&version=$VERSION"; then
        chmod +x "$CLI_PATH"
        printf '%s' "$VERSION" > "$VERSION_PATH"
    else
        echo "[CastToCloud] WARNING: $PLATFORM CLI download failed"
    fi
}

download windows Win64 casttocloud-cli.exe
download macos   Mac   casttocloud-cli
download linux   Linux casttocloud-cli

echo "[CastToCloud] CLI download script finished"
