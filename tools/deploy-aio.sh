#!/usr/bin/env bash
# Extract a Community Shaders AIO archive into the MO2 development mod folder.
#
# Usage:
#   tools/deploy-aio.sh <CommunityShaders_AIO-*.7z|*.zip> [mod-dir]
#
# The default mod-dir is the local SSE 1.5.97 MO2 instance. The script does not
# edit modlist.txt; enable "Community Shaders Dev" in MO2 after the first copy.
set -euo pipefail

ARCHIVE="${1:-}"
MOD_DIR="${2:-<MO2_INSTANCE>/mods/Community Shaders Dev}"

if [[ -z "$ARCHIVE" || ! -f "$ARCHIVE" ]]; then
	printf 'usage: %s <CommunityShaders_AIO archive> [mod-dir]\n' "$0" >&2
	exit 1
fi

if ! command -v 7z >/dev/null 2>&1; then
	printf '7z is required (install p7zip).\n' >&2
	exit 1
fi

tmp_dir="$(mktemp -d)"
trap 'rm -rf "$tmp_dir"' EXIT

7z x -y "$ARCHIVE" -o"$tmp_dir" >/dev/null

# Packages normally extract flat (SKSE/, Shaders/, ...), but tolerate a single
# enclosing directory from a manually zipped build.
extracted="$tmp_dir"
mapfile -t top_level < <(find "$tmp_dir" -mindepth 1 -maxdepth 1 -printf '%p\n')
if [[ "${#top_level[@]}" -eq 1 && -d "${top_level[0]}" ]]; then
	extracted="${top_level[0]}"
fi

mkdir -p "$MOD_DIR"
# No --delete: local shader edits made directly in the deployed mod are kept.
rsync -a "$extracted"/ "$MOD_DIR"/

printf 'Deployed to: %s\n' "$MOD_DIR"
printf 'In MO2: enable "Community Shaders Dev" and make sure no other Community Shaders install is enabled.\n'
printf 'Shader edits in that folder are compiled at runtime; restart Skyrim after changing them.\n'
