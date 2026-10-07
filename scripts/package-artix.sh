#!/usr/bin/env bash
# Build a native pacman package locally, without changing host packages.
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"
source_archive=$(realpath "${1:?usage: package-artix.sh SOURCE_ARCHIVE SHA256 [OUTPUT_DIRECTORY]}")
source_sha=${2:?provide the published source archive SHA-256}
out=$(realpath -m "${3:-release-assets}")
case "$out/" in "$root/"*) ;; *) echo 'Output must be inside the Aeris workspace' >&2; exit 2;; esac
[[ "$source_sha" =~ ^[a-f0-9]{64}$ ]]
printf '%s  %s\n' "$source_sha" "$source_archive" | sha256sum -c -
version=$(sed -n 's/^project(Aeris VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)
[[ $(uname -m) == x86_64 ]] || { echo 'Only Artix x86-64 has package acceptance coverage' >&2; exit 2; }
work="$root/build-artix/$version"
mkdir -p "$work" "$out"
cp "$source_archive" "$work/aeris-v$version-source.tar.gz"
sed -e "s/@VERSION@/$version/g" -e "s/@SHA256@/$source_sha/g" packaging/aur/PKGBUILD.in > "$work/PKGBUILD"
image=${ARTIX_IMAGE:-docker.io/artixlinux/artixlinux:base-devel}
podman pull "$image"
podman image inspect "$image" > "$work/container-image.json"
image_id=$(podman image inspect --format '{{.Id}}' "$image")
podman run --rm --userns=keep-id --user 0 -v "$root:/workspace" \
  -e AERIS_UID="$(id -u)" -e AERIS_VERSION="$version" "$image_id" bash -euo pipefail -c '
    # Reject any image configured with nonstandard repositories.
    repositories=$(pacman-conf --repo-list)
    while read -r repository; do
      case "$repository" in system|world|galaxy) ;; *) echo "Unexpected repository: $repository" >&2; exit 2;; esac
    done <<< "$repositories"
    pacman -Syu --noconfirm --needed base-devel cmake ninja pkgconf qt6-base qtkeychain-qt6 \
      openssl libsodium libzip zxing-cpp protobuf abseil-cpp
    builder=$(getent passwd "$AERIS_UID" | cut -d: -f1) || true
    if [[ -z "$builder" ]]; then
      useradd -m -u "$AERIS_UID" aeris-builder
      builder=aeris-builder
    fi
    work="/workspace/build-artix/$AERIS_VERSION"
    runuser -u "$builder" -- bash -euo pipefail -c "cd \"$work\"; export CMAKE_BUILD_PARALLEL_LEVEL=2; makepkg --cleanbuild --force --noconfirm"
    runuser -u "$builder" -- sh -c "pacman -Q > \"$work/build-packages.txt\"; pacman-conf --repo-list > \"$work/repositories.txt\""
  ' 2>&1 | tee "$work/build.log"
mapfile -t packages < <(find "$work" -maxdepth 1 -name "aeris-$version-2-x86_64.pkg.tar.zst" -type f)
[[ ${#packages[@]} == 1 ]] || { echo 'Expected one native package' >&2; exit 2; }
cp "${packages[0]}" "$out/"
cp "$work/PKGBUILD" "$out/PKGBUILD"
echo "Native package: $out/$(basename "${packages[0]}")"
