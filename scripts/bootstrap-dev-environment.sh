#!/usr/bin/env bash
# One-shot developer environment bootstrap for the Remus workspace.
#
# Primary use: run inside an open Cursor workspace terminal:
#   bash scripts/bootstrap-dev-environment.sh --build
#
# Greenfield (clone + bootstrap):
#   bash scripts/bootstrap-dev-environment.sh --clone-into ~/Cursor/Repos --build
#
# Installs system packages, Cursor/VS Code extensions (Remus profile by default),
# binds this folder to the Remus profile, bootstraps compendium, configures CMake,
# and writes profile-specific editor settings (tool paths, SQLTools absolute DB path).
#
set -euo pipefail

_SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
INITIAL_ROOT="$(cd "${_SCRIPT_DIR}/.." && pwd)"

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
BOLD='\033[1m'
NC='\033[0m'

info() { echo -e "${BLUE}INFO${NC}  $*"; }
ok() { echo -e "${GREEN}OK${NC}    $*"; }
warn() { echo -e "${YELLOW}WARN${NC}  $*"; }
die() { echo -e "${RED}ERROR${NC} $*" >&2; exit 1; }
section() { echo -e "\n${BOLD}== $* ==${NC}"; }

SKIP_PACKAGES=0
SKIP_EXTENSIONS=0
SKIP_COMPENDIUM=0
SKIP_CMAKE=0
SKIP_EDITOR_CONFIG=0
DO_BUILD=0
DO_CLEAN=0
CLONE_INTO=""
REPO_URL="${REMUS_REPO_URL:-}"
CURSOR_PROFILE="${CURSOR_PROFILE:-Remus}"

usage() {
    cat <<'EOF'
bootstrap-dev-environment.sh — prepare a fresh machine for Remus development.

Primary use (Cursor workspace terminal):
  bash scripts/bootstrap-dev-environment.sh --build

Greenfield (clone repository, then bootstrap):
  bash scripts/bootstrap-dev-environment.sh --clone-into ~/Cursor/Repos --build

Steps (in order):
  0. Optional git clone into <parent>/remus
  1. Optional workspace cleanup (audit build trees)
  2. OS packages (CMake, Qt 6, clang, shellcheck, lldb, …)
  3. Cursor/VS Code extensions into the Remus profile
  4. Bind this workspace folder to the Remus profile
  5. Compendium bootstrap DB (create only if missing)
  6. CMake debug preset + compile_commands.json
  7. Profile editor settings (clangd, clang-format, qmlls, SQLTools path)
  8. Optional remus-cli build

Options:
  --clone-into DIR    Clone into DIR/remus before setup (skip when already in repo)
  --repo-url URL      Git remote for clone (default: origin or github.com/asafelobotomy/remus)
  --clean             Remove stale audit build trees before setup
  --skip-packages     Skip OS package installation
  --skip-extensions   Skip Cursor/VS Code extension installation
  --skip-compendium   Skip compendium DB bootstrap
  --skip-cmake        Skip CMake debug preset configure
  --skip-editor-config  Skip extension/tool-path/SQLTools/profile configuration
  --build             Build remus-cli after configure
  --profile NAME      Cursor profile for extensions (default: Remus)
  -h, --help          Show this help

Environment:
  CURSOR_PROFILE      Same as --profile (default: Remus)
  REMUS_REPO_URL      Same as --repo-url
  EDITOR_CMD          Editor CLI on PATH (default: cursor, then code)

Examples:
  bash scripts/bootstrap-dev-environment.sh --build
  bash scripts/bootstrap-dev-environment.sh --clone-into ~/projects --build
  bash scripts/bootstrap-dev-environment.sh --skip-packages --profile Remus

After bootstrap in Cursor: Developer → Reload Window so the Remus profile binding applies.

Optional packages (not installed by default):
  mame      — DAT/listxml refresh scripts
  zstd      — thumbnail archival scripts
  gh        — GitHub release/PR helpers
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --skip-packages) SKIP_PACKAGES=1 ;;
        --skip-extensions) SKIP_EXTENSIONS=1 ;;
        --skip-compendium) SKIP_COMPENDIUM=1 ;;
        --skip-cmake) SKIP_CMAKE=1 ;;
        --skip-editor-config) SKIP_EDITOR_CONFIG=1 ;;
        --clean) DO_CLEAN=1 ;;
        --clone-into)
            [[ $# -ge 2 ]] || die "--clone-into requires a parent directory"
            CLONE_INTO="$2"
            shift
            ;;
        --repo-url)
            [[ $# -ge 2 ]] || die "--repo-url requires a URL"
            REPO_URL="$2"
            shift
            ;;
        --build) DO_BUILD=1 ;;
        --profile)
            [[ $# -ge 2 ]] || die "--profile requires a name"
            CURSOR_PROFILE="$2"
            shift
            ;;
        -h | --help)
            usage
            exit 0
            ;;
        *)
            die "Unknown option: $1 (try --help)"
            ;;
    esac
    shift
done

is_remus_repo() {
    local root="$1"
    [[ -f "${root}/CMakeLists.txt" && -f "${root}/scripts/bootstrap-dev-environment.sh" ]] \
        && grep -q 'project(Remus' "${root}/CMakeLists.txt" 2>/dev/null
}

default_repo_url() {
    if [[ -n "$REPO_URL" ]]; then
        printf '%s\n' "$REPO_URL"
        return 0
    fi
    if git -C "$INITIAL_ROOT" remote get-url origin >/dev/null 2>&1; then
        git -C "$INITIAL_ROOT" remote get-url origin
        return 0
    fi
    printf '%s\n' 'https://github.com/asafelobotomy/remus.git'
}

build_forwarded_args() {
    local args=()
    [[ $DO_CLEAN -eq 1 ]] && args+=(--clean)
    [[ $SKIP_PACKAGES -eq 1 ]] && args+=(--skip-packages)
    [[ $SKIP_EXTENSIONS -eq 1 ]] && args+=(--skip-extensions)
    [[ $SKIP_COMPENDIUM -eq 1 ]] && args+=(--skip-compendium)
    [[ $SKIP_CMAKE -eq 1 ]] && args+=(--skip-cmake)
    [[ $SKIP_EDITOR_CONFIG -eq 1 ]] && args+=(--skip-editor-config)
    [[ $DO_BUILD -eq 1 ]] && args+=(--build)
    if [[ "$CURSOR_PROFILE" != "Remus" ]]; then
        args+=(--profile "$CURSOR_PROFILE")
    fi
    printf '%s\0' "${args[@]}"
}

clone_and_reexec() {
    local parent target repo_url
    parent="$(realpath "$CLONE_INTO")"
    target="${parent}/remus"
    repo_url="$(default_repo_url)"

    section "Clone repository"
    info "Parent directory: ${parent}"
    info "Clone target:     ${target}"
    info "Repository URL:   ${repo_url}"

    command -v git >/dev/null 2>&1 || die "git is required for --clone-into"

    mkdir -p "$parent"
    if [[ -d "${target}/.git" ]]; then
        ok "Using existing clone at ${target}"
    else
        git clone "$repo_url" "$target"
        ok "Cloned into ${target}"
    fi

    mapfile -d '' -t forwarded < <(build_forwarded_args)
    exec bash "${target}/scripts/bootstrap-dev-environment.sh" "${forwarded[@]}"
}

if [[ -n "$CLONE_INTO" ]]; then
    clone_and_reexec
fi

ROOT_DIR="$INITIAL_ROOT"
cd "$ROOT_DIR"

if ! is_remus_repo "$ROOT_DIR"; then
    die "Not inside a Remus repository. Use --clone-into <parent-dir> or open the remus workspace first."
fi

detect_distro() {
    if [[ -f /etc/os-release ]]; then
        # shellcheck disable=SC1091
        . /etc/os-release
        case "${ID:-}${ID_LIKE:-}" in
            *arch* | *cachyos* | *manjaro*) echo arch ;;
            *debian* | *ubuntu*) echo debian ;;
            *fedora*) echo fedora ;;
            *) echo unknown ;;
        esac
        return
    fi
    echo unknown
}

run_as_root() {
    if [[ "$(id -u)" -eq 0 ]]; then
        "$@"
    elif command -v sudo >/dev/null 2>&1; then
        sudo "$@"
    else
        die "Package install requires root or sudo."
    fi
}

install_packages_arch() {
    section "System packages (Arch/CachyOS)"
    run_as_root pacman -S --needed --noconfirm \
        base-devel cmake gcc clang ninja \
        qt6-base qt6-declarative qtkeychain-qt6 \
        zlib libarchive sqlite \
        shellcheck zip unzip p7zip-full \
        lcov lldb ccache jq
    ok "Arch packages installed"
}

install_packages_debian() {
    section "System packages (Debian/Ubuntu)"
    bash "$ROOT_DIR/.github/scripts/install-build-deps.sh"
    run_as_root apt-get install -y lcov lldb ccache jq
    if [[ -x "$ROOT_DIR/.github/scripts/install-clang-format.sh" ]]; then
        bash "$ROOT_DIR/.github/scripts/install-clang-format.sh"
    fi
    ok "Debian/Ubuntu packages installed"
}

install_packages_fedora() {
    section "System packages (Fedora)"
    run_as_root dnf install -y \
        cmake qt6-qtbase-devel qt6-qtbase-private-devel qt6-qtdeclarative-devel \
        zlib-devel libarchive-devel qt6-qtkeychain-devel gcc-c++ clang clang-tools-extra \
        ninja-build sqlite shellcheck zip unzip p7zip p7zip-plugins \
        lcov lldb ccache jq
    ok "Fedora packages installed"
}

install_packages() {
    local distro
    distro="$(detect_distro)"
    info "Detected distro family: ${distro}"

    case "$distro" in
        arch) install_packages_arch ;;
        debian) install_packages_debian ;;
        fedora) install_packages_fedora ;;
        *)
            die "Unsupported distro for automatic package install. Install deps from docs/setup/BUILD.md and re-run with --skip-packages."
            ;;
    esac
}

find_editor_cmd() {
    if [[ -n "${EDITOR_CMD:-}" ]] && command -v "$EDITOR_CMD" >/dev/null 2>&1; then
        echo "$EDITOR_CMD"
        return
    fi
    for candidate in cursor code; do
        if command -v "$candidate" >/dev/null 2>&1; then
            echo "$candidate"
            return
        fi
    done
    return 1
}

read_extension_ids() {
    local extensions_file="$ROOT_DIR/.vscode/extensions.json"
    [[ -f "$extensions_file" ]] || die "Missing $extensions_file"

    if command -v jq >/dev/null 2>&1; then
        jq -r '.recommendations[]' "$extensions_file"
        return
    fi

    warn "jq not found; using built-in extension list"
    cat <<'EOF'
ms-vscode.cmake-tools
llvm-vs-code-extensions.vscode-clangd
xaver.clang-format
theqtcompany.qt-qml
timonwong.shellcheck
vadimcn.vscode-lldb
redhat.vscode-yaml
fredericbonnet.cmake-test-adapter
mtxr.sqltools
mtxr.sqltools-driver-sqlite
davidanson.vscode-markdownlint
EOF
}

clean_workspace() {
    section "Workspace cleanup"
    bash "$ROOT_DIR/scripts/clean-workspace.sh"
    ok "Stale audit build trees removed"
}

install_extensions() {
    section "Cursor/VS Code extensions (profile: ${CURSOR_PROFILE})"

    local editor_cmd ext_ids=()
    editor_cmd="$(find_editor_cmd)" || die "No cursor or code CLI on PATH. Install Cursor, then re-run with --skip-packages if system deps are already present."

    mapfile -t ext_ids < <(read_extension_ids)
    [[ ${#ext_ids[@]} -gt 0 ]] || die "No extension IDs to install"

    local install_args=()
    for ext in "${ext_ids[@]}"; do
        install_args+=(--install-extension "$ext")
    done

    info "Using ${editor_cmd} --profile ${CURSOR_PROFILE}"

    "$editor_cmd" --profile "$CURSOR_PROFILE" "${install_args[@]}"

    ok "Extensions installed into profile '${CURSOR_PROFILE}'"
    info "Open this workspace with: ${editor_cmd} --profile ${CURSOR_PROFILE} ${ROOT_DIR}"
}

configure_editor_extensions() {
    section "Extension configuration"
    bash "$ROOT_DIR/scripts/configure-workspace-extensions.sh"
}

bootstrap_compendium() {
    section "Compendium bootstrap"
    local db_path
    db_path="$(bash "$ROOT_DIR/scripts/resolve_compendium_db.sh" --ensure)"
    ok "Compendium ready at ${db_path}"
}

configure_cmake() {
    section "CMake debug preset"
    bash "$ROOT_DIR/.github/scripts/cmake-configure-debug.sh"
    ok "build-debug/ configured; compile_commands.json ready for clangd"
}

build_cli() {
    section "Build remus-cli (debug)"
    cmake --build build-debug --target remus-cli -j"$(nproc)"
    ok "remus-cli built: build-debug/remus-cli"
    build-debug/remus-cli --version
}

verify_toolchain() {
    section "Toolchain check"
    local missing=() tool
    for tool in cmake c++ clang-format clangd shellcheck sqlite3 7z; do
        if ! command -v "$tool" >/dev/null 2>&1; then
            missing+=("$tool")
        else
            info "$tool → $(command -v "$tool")"
        fi
    done

    for candidate in /usr/lib/qt6/bin/qmllint /usr/bin/qmllint; do
        if [[ -x "$candidate" ]]; then
            info "qmllint → $candidate"
            break
        fi
    done

    if [[ ${#missing[@]} -gt 0 ]]; then
        warn "Still missing: ${missing[*]}"
        return 1
    fi
    ok "Core toolchain present"
}

main() {
    section "Remus dev environment bootstrap"
    info "Repository: ${ROOT_DIR}"

    if [[ "$DO_CLEAN" -eq 1 ]]; then
        clean_workspace
    fi

    if [[ "$SKIP_PACKAGES" -eq 0 ]]; then
        install_packages
    else
        info "Skipping package install"
    fi

    if [[ "$SKIP_EXTENSIONS" -eq 0 ]]; then
        install_extensions
    else
        info "Skipping extension install"
    fi

    if [[ "$SKIP_COMPENDIUM" -eq 0 ]]; then
        bootstrap_compendium
    else
        info "Skipping compendium bootstrap"
    fi

    if [[ "$SKIP_CMAKE" -eq 0 ]]; then
        configure_cmake
    else
        info "Skipping CMake configure"
    fi

    if [[ "$SKIP_EDITOR_CONFIG" -eq 0 ]]; then
        configure_editor_extensions
    else
        info "Skipping extension configuration"
    fi

    if [[ "$DO_BUILD" -eq 1 ]]; then
        build_cli
    fi

    verify_toolchain || true

    section "Done"
    ok "Bootstrap complete."
    cat <<EOF

Next steps:
  1. In Cursor: Developer → Reload Window (applies Remus profile binding)
  2. Build:  cmake --build build-debug -j\$(nproc)
  3. Test:   ctest --test-dir build-debug --output-on-failure
  4. Audit:  bash scripts/run-local-audit.sh

Open elsewhere: $(find_editor_cmd 2>/dev/null || echo cursor) --profile ${CURSOR_PROFILE} ${ROOT_DIR}

EOF
}

main "$@"
