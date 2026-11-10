# Workstation build routing

The user requested that Aeris build and release work use the station at `/run/media/user/edat/VMs`. Do not enable or run GitHub Actions.

Use `scripts/build-station --target TARGET [buildlab run options] -- COMMAND ARG...` from this checkout. This submits the current repository, including uncommitted and nonignored untracked source files, through BuildLab. Do not supply `--ref` when current edits are required. Inside a BuildLab worker, execute build commands directly instead of calling the launcher again.

Read the station's `AGENTS.md`, `README.md`, and `HANDOFF.md`; run its `buildlab doctor`, `buildlab targets`, and `buildlab status` before starting jobs. Use ready targets, the shared lock, and normal memory admission. Use project scripts for multistep commands. For incremental work, pass a distinct `--workspace NAME` for each checkout, platform, and configuration. Write deliverables to `BUILDLAB_ARTIFACTS`. Run `buildlab workspace reset NAME --target TARGET --repo REPO` when a specific workspace is no longer needed.

Aeris needs Qt 6.4+, QtKeychain 0.14+, ZXing-C++ 2.2+, OpenSSL 3, protobuf, libsodium, and libzip. Verify the selected worker's dependencies before configuring. Use scripts/build-local.py for native Windows/macOS BuildLab jobs; its release-assets output is redirected to station storage.

Generated build, dependency, package, and environment directories were relocated under `/run/media/user/edat/VMs/.state/project-storage/projects/aeris/`. Their original paths are directory symlinks. Keep those links intact, keep the station drive mounted, and avoid creating new local build trees. Git's local `.git/info/exclude` omits these links from status and BuildLab snapshots.

CMake object/autogen directories were removed during cleanup. Retained builds need a fresh configure before rebuilding; prefer a fresh BuildLab workspace. Packages, executables, logs, screenshots, recovery repositories, and source snapshots were retained. Cleanup manifests and source hashes are in the station's `.state/project-storage/maintenance/`.

`/home/user/Documents/CODE/aeris` is the sole active checkout after consolidation requested by the user on 2026-11-09. The former uppercase checkout is preserved in `.git/folder-consolidation-20261109/Aeris.tar.gz`, including its original Git metadata and build symlinks; the adjacent `manifest.json` records verified file hashes. Preserve this local recovery archive and all preexisting uncommitted work. Do not recreate the uppercase sibling folder.
