#!/usr/bin/env bash
set -euo pipefail

destination=${1:-build/windows64-deps}
sdl_version=2.32.10
image_version=2.8.12
sdl_archive="SDL2-devel-${sdl_version}-mingw.tar.gz"
image_archive="SDL2_image-devel-${image_version}-mingw.tar.gz"
sdl_sha256=83a5d74012311edc3c0d40ea6faecbe57ad692aa033fa5dc273cc937e3938ff2
image_sha256=85a03645d05c59aa3e76a05347fb94887e4e34d23bae6dd22aa3571a2dde9012

mkdir -p "$destination/downloads"

fetch_archive() {
    local archive=$1
    local url=$2
    local expected=$3
    local path="$destination/downloads/$archive"
    local partial="$path.part"

    if [[ -f "$path" ]] && ! printf '%s  %s\n' "$expected" "$path" | sha256sum --check --status; then
        rm -f "$path"
    fi
    if [[ ! -f "$path" ]]; then
        curl --fail --location --retry 3 --continue-at - --output "$partial" "$url"
        printf '%s  %s\n' "$expected" "$partial" | sha256sum --check --status
        mv "$partial" "$path"
    fi
}

fetch_archive "$sdl_archive" \
    "https://github.com/libsdl-org/SDL/releases/download/release-${sdl_version}/${sdl_archive}" \
    "$sdl_sha256"
fetch_archive "$image_archive" \
    "https://github.com/libsdl-org/SDL_image/releases/download/release-${image_version}/${image_archive}" \
    "$image_sha256"

if [[ ! -d "$destination/SDL2-${sdl_version}" ]]; then
    tar -xzf "$destination/downloads/$sdl_archive" -C "$destination"
fi
if [[ ! -d "$destination/SDL2_image-${image_version}" ]]; then
    tar -xzf "$destination/downloads/$image_archive" -C "$destination"
fi
