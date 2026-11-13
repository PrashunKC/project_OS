#!/usr/bin/env bash
#
# NBOS build, image, and emulator diagnostic test.
#
# This script is intentionally non-destructive: it does not unload KVM modules,
# delete VirtualBox VMs, or modify host configuration. Every command is logged.

set -uo pipefail

ROOT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${ROOT_DIR}/build"
LOG_ROOT="${BUILD_DIR}/diagnostics"
RUN_ID="$(date -u +%Y%m%dT%H%M%SZ)"
RUN_DIR="${LOG_ROOT}/${RUN_ID}"
LOG_FILE="${RUN_DIR}/test.log"
SUMMARY_FILE="${RUN_DIR}/summary.txt"
WARNINGS_FILE="${RUN_DIR}/warnings.txt"
ERRORS_FILE="${RUN_DIR}/errors.txt"
QEMU_TIMEOUT="${QEMU_TIMEOUT:-12}"
RUN_OPTIONAL=1
RUN_VIRTUALBOX=0
RUN_VIRTUALBOX_BOOT=0
DO_CLEAN=0
FAILURES=0
SKIPS=0
WARNINGS=0

usage() {
    cat <<'EOF'
Usage: ./test.sh [options]

Runs tool checks, builds every supported image, validates artifacts, and
performs bounded QEMU boot observations. All output is saved under
build/diagnostics/<UTC timestamp>/.

Options:
  --clean          Run make clean before testing.
  --no-optional    Skip ISO, HDD, GRUB, and SDK-adjacent optional targets.
  --virtualbox     Also collect VirtualBox host diagnostics (does not start VM).
  --virtualbox-boot
                   Start run-vbox and verify a live VM plus screen/log evidence.
  --qemu-timeout N  Seconds to observe each QEMU boot (default: 12).
  -h, --help       Show this help.
EOF
}

while (($#)); do
    case "$1" in
        --clean) DO_CLEAN=1 ;;
        --no-optional) RUN_OPTIONAL=0 ;;
        --virtualbox) RUN_VIRTUALBOX=1 ;;
        --virtualbox-boot)
            RUN_VIRTUALBOX=1
            RUN_VIRTUALBOX_BOOT=1
            ;;
        --qemu-timeout)
            shift
            [[ "${1:-}" =~ ^[0-9]+$ ]] || { echo "Invalid --qemu-timeout value" >&2; exit 2; }
            QEMU_TIMEOUT="$1"
            ;;
        -h|--help) usage; exit 0 ;;
        *) echo "Unknown option: $1" >&2; usage >&2; exit 2 ;;
    esac
    shift
done

mkdir -p "$RUN_DIR"
touch "$LOG_FILE" "$SUMMARY_FILE" "$WARNINGS_FILE" "$ERRORS_FILE"
exec > >(tee -a "$LOG_FILE") 2>&1

log() { printf '[%s] %s\n' "$(date -u +%H:%M:%S)" "$*"; }
record() {
    printf '%-8s %s\n' "$1" "$2" | tee -a "$SUMMARY_FILE"
    if [[ "$1" == "FAIL" ]]; then
        printf '%s\n' "$2" >> "$ERRORS_FILE"
    fi
}
warn() {
    printf '%-8s %s\n' "WARNING" "$*" | tee -a "$SUMMARY_FILE"
    printf '%s\n' "$*" >> "$WARNINGS_FILE"
    WARNINGS=$((WARNINGS + 1))
}

run_step() {
    local name="$1"
    shift
    log "TEST ${name}: $*"
    if "$@"; then
        record PASS "$name"
        return 0
    fi
    record FAIL "$name"
    FAILURES=$((FAILURES + 1))
    return 1
}

run_shell_step() {
    local name="$1"
    local command="$2"
    log "TEST ${name}: ${command}"
    if bash -c "$command"; then
        record PASS "$name"
        return 0
    fi
    record FAIL "$name"
    FAILURES=$((FAILURES + 1))
    return 1
}

optional_shell_step() {
    local name="$1"
    local command="$2"
    if [[ "$RUN_OPTIONAL" -eq 0 ]]; then
        record SKIP "$name (disabled)"
        SKIPS=$((SKIPS + 1))
        return 0
    fi
    run_shell_step "$name" "$command"
}

capture_host_state() {
    log "Diagnostic output directory: ${RUN_DIR}"
    {
        printf 'UTC: '; date -u
        printf 'Root: %s\n' "$ROOT_DIR"
        printf 'Kernel: '; uname -a
        printf 'User: '; id
        printf 'PATH: %s\n' "$PATH"
        printf '\nGit status:\n'
        git -C "$ROOT_DIR" status --short --branch 2>&1 || true
        printf '\nEnvironment:\n'
        env | sort
    } | tee "${RUN_DIR}/host.txt"

    for tool in make nasm gcc ld objcopy objdump mkfs.fat mcopy qemu-system-x86_64 \
        grub-file grub-mkrescue xorriso VBoxManage timeout python3; do
        if command -v "$tool" >/dev/null 2>&1; then
            printf 'FOUND %-24s %s\n' "$tool" "$(command -v "$tool")"
            "$tool" --version 2>&1 | head -n 2 || true
        else
            printf 'MISSING %s\n' "$tool"
        fi
    done | tee "${RUN_DIR}/tools.txt"

    {
        printf '\nVirtualization:\n'
        command -v lsmod >/dev/null 2>&1 && lsmod | grep -E '^(kvm|vbox)' || true
        [[ -e /dev/kvm ]] && ls -l /dev/kvm || echo '/dev/kvm: unavailable'
        command -v lscpu >/dev/null 2>&1 && lscpu | grep -E 'Virtualization|Hypervisor' || true
        if command -v VBoxManage >/dev/null 2>&1; then
            VBoxManage list hostinfo 2>&1 || true
            VBoxManage list runningvms 2>&1 || true
        fi
    } | tee "${RUN_DIR}/virtualization.txt"
}

make_logged() {
    local name="$1"
    shift
    run_shell_step "$name" "cd '$ROOT_DIR' && make $*"
}

validate_artifacts() {
    local file
    for file in build/main_floppy.img build/stage1.bin build/stage2.bin \
        build/kernel.bin build/kernel.elf build/tools/fat; do
        [[ -f "${ROOT_DIR}/${file}" ]] || {
            log "Missing required artifact: ${file}"
            return 1
        }
        file "${ROOT_DIR}/${file}"
    done
    if command -v grub-file >/dev/null 2>&1; then
        grub-file --is-x86-multiboot "${ROOT_DIR}/build/kernel.elf"
    fi
    if command -v mdir >/dev/null 2>&1; then
        mdir -i "${ROOT_DIR}/build/main_floppy.img" ::/
    elif [[ -x "${ROOT_DIR}/build/tools/fat" ]]; then
        "${ROOT_DIR}/build/tools/fat" "${ROOT_DIR}/build/main_floppy.img" "KERNEL  BIN"
    fi
    od -An -tx1 -j510 -N2 "${ROOT_DIR}/build/main_hdd.img" 2>/dev/null || true
}

validate_build_recipes() {
    local qemu_recipe iso_recipe
    qemu_recipe="$(make -C "$ROOT_DIR" -n qemu-test 2>&1)" || return 1
    grep -Fq -- "-drive format=raw,if=floppy,file=build/main_floppy.img" \
        <<<"$qemu_recipe" || {
        log "qemu-test does not boot the custom floppy image"
        return 1
    }
    if grep -Fq -- "-kernel build/kernel.bin" <<<"$qemu_recipe"; then
        log "qemu-test still passes the flat kernel to QEMU's Linux loader"
        return 1
    fi

    iso_recipe="$(make -C "$ROOT_DIR" -n iso_noemul 2>&1)" || return 1
    grep -Fq -- "-b main_floppy.img" <<<"$iso_recipe" || {
        log "iso_noemul does not use the known-good floppy boot image"
        return 1
    }
    if grep -Fq -- "-no-emul-boot" <<<"$iso_recipe"; then
        log "iso_noemul still embeds a FAT VBR as a no-emulation boot image"
        return 1
    fi

    grep -Fq "set gfxpayload=text" "$ROOT_DIR/Makefile" || {
        log "GRUB target does not request the compatible text payload"
        return 1
    }
    if grep -Fq "'insmod vbe'" "$ROOT_DIR/Makefile"; then
        log "GRUB target still forces the unsupported VBE module"
        return 1
    fi
}

validate_kernel_entry() {
    local entry_offset normalized_offset
    entry_offset="$(nm -n "$ROOT_DIR/build/kernel.elf" |
        awk '$3 == "_start" { print "0x" $1; exit }')" || return 1
    normalized_offset="$(printf '0x%x' "$((entry_offset))")" || return 1
    [[ "$normalized_offset" == "0x100020" ]] || {
        log "kernel _start is ${normalized_offset}, expected 0x100020"
        return 1
    }
    grep -Fq "#define KERNEL_ENTRY_OFFSET 32" \
        "$ROOT_DIR/src/bootloader/stage2/main.c" || {
        log "stage2 entry offset is not synchronized with the linked kernel"
        return 1
    }
}

qemu_observe() {
    local name="$1"
    local image_args="$2"
    local socket="${RUN_DIR}/${name}.monitor"
    local screenshot="${RUN_DIR}/${name}.ppm"
    local terminal_screenshot="${RUN_DIR}/${name}-terminal.ppm"
    local qemu_log="${RUN_DIR}/${name}.qemu.log"
    local monitor_log="${RUN_DIR}/${name}.monitor.log"
    local pid

    command -v qemu-system-x86_64 >/dev/null 2>&1 || {
        record SKIP "${name} (qemu-system-x86_64 unavailable)"
        SKIPS=$((SKIPS + 1))
        return 0
    }

    rm -f "$socket" "$screenshot" "$terminal_screenshot" "$monitor_log"
    log "TEST ${name}: observing QEMU for ${QEMU_TIMEOUT}s"
    (
        cd "$ROOT_DIR"
        timeout --signal=TERM --kill-after=3 "${QEMU_TIMEOUT}s" \
            qemu-system-x86_64 $image_args \
            -vga std \
            -display none -monitor "unix:${socket},server,nowait" \
            -no-reboot -no-shutdown -d guest_errors,cpu_reset -D "$qemu_log" \
            >>"$qemu_log" 2>&1
    ) &
    pid=$!

    for _ in {1..30}; do
        [[ -S "$socket" ]] && break
        sleep 0.2
    done
    if [[ -S "$socket" ]]; then
        if ! python3 - "$socket" "$screenshot" "$terminal_screenshot" "$monitor_log" <<'PY'
import socket
import sys
import time

sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
sock.settimeout(3)
sock.connect(sys.argv[1])
time.sleep(0.3)
response = bytearray()
try:
    sock.sendall(("screendump " + sys.argv[2] + "\n").encode())
    time.sleep(0.5)
    sock.sendall(b"sendkey ret\n")
    time.sleep(0.5)
    sock.sendall(("screendump " + sys.argv[3] + "\n").encode())
    time.sleep(0.5)
    sock.sendall(b"quit\n")
except BrokenPipeError:
    response.extend(b"monitor closed before all commands completed\n")
else:
    try:
        while True:
            chunk = sock.recv(4096)
            if not chunk:
                break
            response.extend(chunk)
    except socket.timeout:
        pass
sock.close()
with open(sys.argv[4], "wb") as output:
    output.write(response)
PY
        then
            warn "${name}: monitor command exchange failed; inspect ${monitor_log}"
        fi
        if [[ ! -s "$monitor_log" ]]; then
            warn "${name}: monitor returned no response; screenshot evidence may be incomplete"
        fi
    else
        warn "${name}: QEMU monitor socket was not created"
    fi
    wait "$pid"
    local status=$?
    if [[ "$status" -eq 124 || "$status" -eq 137 ]]; then
        log "${name}: QEMU remained running until the observation timeout (expected for a kernel)."
    elif [[ "$status" -ne 0 ]]; then
        record FAIL "${name} (QEMU exit ${status})"
        FAILURES=$((FAILURES + 1))
        return 1
    fi
    [[ -s "$qemu_log" ]] || log "${name}: QEMU produced no diagnostic log."
    if grep -Eiq \
        'error while loading elf kernel|multiboot knows vbe|unsupported graphical mode|you need to load the kernel first|load disk|read err|no stage2|guru meditation|triple fault' \
        "$qemu_log"; then
        record FAIL "${name} (boot error signature)"
        FAILURES=$((FAILURES + 1))
        return 1
    fi
    if grep -Eiq \
        'warning|deprecated|unsupported|failed|not found|no bootable|reset|read err|load disk|no stage2' \
        "$qemu_log" "$monitor_log" 2>/dev/null; then
        while IFS= read -r line; do
            [[ "$line" =~ terminating\ on\ signal ]] && continue
            warn "${name}: ${line}"
        done < <(grep -hEi \
            'warning|deprecated|unsupported|failed|not found|no bootable|reset|read err|load disk|no stage2' \
            "$qemu_log" "$monitor_log" 2>/dev/null | sort -u | head -20)
    fi
    [[ -s "$screenshot" ]] && file "$screenshot" || log "${name}: no screenshot captured."
    [[ -s "$terminal_screenshot" ]] && file "$terminal_screenshot" ||
        log "${name}: no post-welcome terminal screenshot captured."
    record PASS "$name (boot observed; see ${RUN_DIR})"
}

analyze_diagnostics() {
    local line
    if grep -Eiq 'warning:' "$LOG_FILE"; then
        while IFS= read -r line; do
            warn "build: ${line}"
        done < <(grep -Ei 'warning:' "$LOG_FILE" | sort -u | head -20)
    fi
    if grep -Eiq 'missing|unavailable' "$RUN_DIR/tools.txt"; then
        while IFS= read -r line; do
            warn "host: ${line}"
        done < <(grep -Ei 'missing|unavailable' "$RUN_DIR/tools.txt" | sort -u)
    fi
    if [[ -f "${RUN_DIR}/virtualbox-vminfo.txt" ]] &&
        grep -Eiv 'Triple Fault Reset:' "${RUN_DIR}/virtualbox-vminfo.txt" |
            grep -Eiq 'Guru Meditation|VINF_EM_TRIPLE_FAULT|fatal|aborted|VERR_SVM_IN_USE|VERR_VMX_IN_USE'; then
        warn "virtualbox: VM information contains a fault or stuck state"
    fi
    if [[ "$WARNINGS" -gt 0 ]]; then
        log "Diagnostic interpretation found ${WARNINGS} warning(s); warnings do not imply failure."
    else
        log "Diagnostic interpretation found no warning signatures."
    fi
}

virtualbox_boot_check() {
    local vm_state=""
    local screenshot="${RUN_DIR}/virtualbox-screen.png"
    local vm_log="${RUN_DIR}/virtualbox-VBox.log"
    local vminfo="${RUN_DIR}/virtualbox-vminfo.txt"
    local fault_signature=0
    local line
    if ! command -v VBoxManage >/dev/null 2>&1; then
        record SKIP "virtualbox_boot (VBoxManage unavailable)"
        SKIPS=$((SKIPS + 1))
        return 0
    fi
    log "TEST virtualbox_boot: make run-vbox"
    if ! (cd "$ROOT_DIR" && timeout 30s make run-vbox >"${RUN_DIR}/virtualbox-boot.log" 2>&1); then
        record FAIL "virtualbox_boot (make run-vbox failed)"
        FAILURES=$((FAILURES + 1))
        return 1
    fi
    for _ in {1..15}; do
        vm_state="$(VBoxManage showvminfo NBOS --machinereadable 2>/dev/null |
            awk -F= '$1 == "VMState" { gsub(/"/, "", $2); print $2; exit }')"
        [[ "$vm_state" == "running" ]] && break
        sleep 1
    done
    VBoxManage showvminfo NBOS >"$vminfo" 2>&1 || true
    VBoxManage controlvm NBOS screenshotpng "$screenshot" >"${RUN_DIR}/virtualbox-screenshot.log" 2>&1 || true
    if [[ -f "${HOME}/VirtualBox VMs/NBOS/Logs/VBox.log" ]]; then
        cp "${HOME}/VirtualBox VMs/NBOS/Logs/VBox.log" "$vm_log"
    fi
    while IFS= read -r line; do
        [[ "$line" =~ [Tt]riple[[:space:]]+[Ff]ault[[:space:]]+[Rr]eset: ]] && continue
        [[ "$line" =~ VERR_HGCM_SERVICE_NOT_FOUND ]] && continue
        fault_signature=1
        break
    done < <(grep -Eih \
        'Guru Meditation|VINF_EM_TRIPLE_FAULT|triple fault|fatal|aborted|VERR_SVM_IN_USE|VERR_VMX_IN_USE|VBOX_E_IPRT_ERROR' \
        "${RUN_DIR}/virtualbox-boot.log" "$vminfo" "$vm_log" 2>/dev/null)
    if [[ "$fault_signature" -eq 1 ]]; then
        record FAIL "virtualbox_boot (fault signature)"
        FAILURES=$((FAILURES + 1))
    elif [[ "$vm_state" != "running" ]]; then
        record FAIL "virtualbox_boot (VM state: ${vm_state:-unknown})"
        FAILURES=$((FAILURES + 1))
    elif [[ ! -s "$screenshot" ]]; then
        record FAIL "virtualbox_boot (no screen capture)"
        FAILURES=$((FAILURES + 1))
    else
        file "$screenshot"
        record PASS "virtualbox_boot (running with screen/log evidence)"
    fi
    VBoxManage controlvm NBOS poweroff >/dev/null 2>&1 || true
}

main() {
    log "NBOS diagnostic test started"
    capture_host_state
    record INFO "Logs: ${RUN_DIR}"

    if [[ "$DO_CLEAN" -eq 1 ]]; then
        make_logged clean "clean"
    fi

    make_logged build "all"
    make_logged stage1 "stage1"
    make_logged stage2 "stage2"
    make_logged kernel "kernel"
    make_logged floppy "floppy_image"
    make_logged tools "tools_fat"
    make_logged sdk "sdk"

    optional_shell_step "iso" "cd '$ROOT_DIR' && make iso"
    optional_shell_step "iso_noemul" "cd '$ROOT_DIR' && make iso_noemul"
    optional_shell_step "hdd_image" "cd '$ROOT_DIR' && make hdd_image"
    optional_shell_step "grub_iso" "cd '$ROOT_DIR' && make grub-iso"

    run_shell_step "artifact_validation" "ROOT_DIR='$ROOT_DIR'; cd '$ROOT_DIR' && $(declare -f log validate_artifacts); validate_artifacts"
    run_shell_step "build_recipe_validation" "ROOT_DIR='$ROOT_DIR'; $(declare -f log validate_build_recipes); validate_build_recipes"
    run_shell_step "kernel_entry_validation" "ROOT_DIR='$ROOT_DIR'; $(declare -f log validate_kernel_entry); validate_kernel_entry"
    qemu_observe floppy "-drive file=${BUILD_DIR}/main_floppy.img,format=raw,if=floppy"
    if [[ "$RUN_OPTIONAL" -eq 1 && -f "${BUILD_DIR}/project_os.iso" ]]; then
        qemu_observe grub "-cdrom ${BUILD_DIR}/project_os.iso"
    else
        record SKIP "grub_qemu (GRUB ISO unavailable)"
        SKIPS=$((SKIPS + 1))
    fi

    if [[ "$RUN_VIRTUALBOX" -eq 1 ]]; then
        if command -v VBoxManage >/dev/null 2>&1; then
            VBoxManage list hostinfo > "${RUN_DIR}/virtualbox-hostinfo.txt" 2>&1 || true
            VBoxManage list runningvms > "${RUN_DIR}/virtualbox-runningvms.txt" 2>&1 || true
            record INFO "VirtualBox diagnostics captured (no VM started)"
        else
            record SKIP "virtualbox (VBoxManage unavailable)"
            SKIPS=$((SKIPS + 1))
        fi
    fi
    if [[ "$RUN_VIRTUALBOX_BOOT" -eq 1 ]]; then
        virtualbox_boot_check
    fi

    {
        analyze_diagnostics
        printf '\nFailures: %d\nWarnings: %d\nSkips: %d\n' \
            "$FAILURES" "$WARNINGS" "$SKIPS"
        printf 'Artifacts and logs: %s\n' "$RUN_DIR"
    } | tee -a "$SUMMARY_FILE"
    [[ "$FAILURES" -eq 0 ]]
}

main "$@"
