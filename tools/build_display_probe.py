"""Build exact-image DRM research modules; never install them.

Generated contracts, original-image inputs and outputs stay in a private work
directory. No force-load or direct code patching. Carrier parameters can
explicitly select a factory commit/recovery or original-value register echo.
The optional observer registers passive counters through the tracepoint API.
The standard module metadata contains only imports present in pinned symvers.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
DDC_IMPORTS = {'__stack_chk_fail','__stack_chk_guard','copy_from_kernel_nofault',
    'i2c_get_adapter','i2c_put_adapter','strcmp','printk','param_ops_bool','param_ops_uint',
    'mutex_lock','mutex_unlock','__module_get','regmap_update_bits_base','regmap_read',
    'regmap_write','wait_for_completion_timeout','memcpy'}
PIN = "d9b764ad8342ce71ddde45bc0978e548ae636ce6d01fb4bf3f426eba5fff0740"
BASE = 0xffffffc010000000
NAME = "rk_display_probe"
TEMPLATE_SHA = "14d047bba8011ac40d7b403ee0da684418261914945b42a07252d574ceb88a55"
IMPORTS = {
    "__stack_chk_fail", "__stack_chk_guard", "bus_find_device",
    "copy_from_kernel_nofault", "drm_connector_list_iter_begin",
    "drm_connector_list_iter_next", "drm_connector_list_iter_end",
    "drm_crtc_from_index", "mutex_trylock", "mutex_unlock",
    "param_ops_bool", "param_ops_int", "platform_bus_type", "printk",
    "put_device", "strcmp",
}
QMS_ABI_IMPORTS = IMPORTS | {"regmap_get_device", "regmap_get_val_bytes", "regmap_get_reg_stride",
                           "drm_mode_convert_to_umode", "drm_modeset_lock_all", "drm_modeset_unlock_all",
                           "drm_atomic_state_alloc", "drm_atomic_get_crtc_state", "__drm_atomic_state_free",
                           "cpu_hwcap_keys", "arm64_const_caps_ready", "refcount_warn_saturate"}
CARRIER_IMPORTS = QMS_ABI_IMPORTS | {"drm_atomic_check_only", "drm_atomic_add_affected_planes",
                                   "drm_atomic_commit", "drm_crtc_vblank_count", "ktime_get",
                                   "bcmp", "param_ops_ulong", "mutex_lock", "regmap_read", "regmap_write",
                                   "regmap_update_bits_base", "_raw_spin_lock", "_raw_spin_unlock",
                                   "drm_atomic_add_affected_connectors", "drm_crtc_vblank_count_and_time", "memcpy", "memset"}
LINK_IMPORTS = CARRIER_IMPORTS | {"drm_property_lookup_blob", "drm_property_blob_put", "clk_get_rate"}
CONTROL_IMPORTS = {'__mutex_init','mutex_lock','mutex_trylock','mutex_unlock','_raw_spin_lock_irqsave',
    '_raw_spin_unlock_irqrestore','kthread_create_on_node','wake_up_process','kthread_should_stop',
    'kthread_stop','msleep','ktime_get','memcpy','misc_register','misc_deregister','_copy_from_user',
    '_copy_to_user','__check_object_size','__stack_chk_fail','__stack_chk_guard','__arch_copy_from_user',
    '__arch_copy_to_user','arm64_const_caps_ready','cpu_hwcap_keys','cpu_hwcaps','gic_nonsecure_priorities',
    'gic_pmr_sync','memset','drm_crtc_vblank_get','drm_crtc_vblank_put',
    '__init_waitqueue_head','init_wait_entry','prepare_to_wait_event','finish_wait','schedule_timeout','__wake_up'}
OWNER_IMPORTS = LINK_IMPORTS | CONTROL_IMPORTS | {'__module_get','drm_dev_get','drm_dev_put'}
OBSERVER_IMPORTS = {"__stack_chk_fail", "__stack_chk_guard", "copy_from_kernel_nofault",
                    "for_each_kernel_tracepoint", "tracepoint_probe_register", "tracepoint_probe_unregister",
                    "tracepoint_srcu", "synchronize_srcu", "synchronize_rcu", "printk", "strcmp",
                    "arm64_const_caps_ready", "cpu_hwcap_keys",
                    "msleep",
                    "scnprintf", "param_ops_bool", "param_ops_int"}


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def require(condition, message):
    # Integrity checks must remain active under python -O.
    if not condition:
        raise RuntimeError(message)


def fnv(data):
    value = 14695981039346656037
    for byte in data:
        value = ((value ^ byte) * 1099511628211) & ((1 << 64) - 1)
    return value


def contract(image, elf, output, carrier=False, live_link=False, functional=False):
    from elftools.elf.elffile import ELFFile
    if sha(image) != PIN:
        raise RuntimeError("Original kernel image SHA256 mismatch")
    raw = image.read_bytes()
    with elf.open("rb") as stream:
        symbols = list(ELFFile(stream).get_section_by_name(".symtab").iter_symbols())
    byname = {}
    for symbol in symbols:
        byname.setdefault(symbol.name, []).append(symbol["st_value"])

    def unique(name):
        values = set(byname[name])
        if len(values) != 1:
            raise RuntimeError("Ambiguous symbol: " + name)
        return values.pop()

    anchor = unique("printk")
    require(anchor - BASE == 0x127fea8, "Unexpected printk anchor")
    lookup = unique("kallsyms_lookup_name")
    require(lookup - anchor == -18205344, "Unexpected lookup offset")
    functions = ["printk", "kallsyms_lookup_name", "rockchip_drm_bind",
                 "dw_hdmi_rockchip_bind", "dw_hdmi_qp_bind", "vop2_bind",
                 "drm_connector_list_iter_begin", "drm_connector_list_iter_next",
                 "drm_connector_list_iter_end", "drm_crtc_from_index"]
    functions += ["for_each_kernel_tracepoint", "tracepoint_probe_register", "tracepoint_probe_unregister", "__traceiter_drm_vblank_event"]
    functions += ["drm_mode_convert_to_umode", "regmap_get_device", "regmap_get_val_bytes",
                  "regmap_get_reg_stride", "regmap_lock_spinlock", "regmap_unlock_spinlock"]
    functions += ["drm_modeset_lock_all", "drm_modeset_unlock_all", "drm_atomic_state_alloc",
                  "drm_atomic_state_init", "drm_atomic_get_crtc_state", "__drm_atomic_state_free"]
    lines = ["/* Generated privately from the pinned original Image. */",
             "#define DISPLAY_LOOKUP_DELTA (%dL)" % (lookup - anchor),
             "static const struct { long delta; unsigned int bytes; u64 fnv, jump_label_alternate; } display_functions[] = {"]
    for name in functions:
        addr = unique(name)
        data = raw[addr - BASE:addr - BASE + 32]
        require(len(data) == 32, "Fingerprint outside Image")
        lines.append("    { %dL, 32, 0x%016xULL, 0 }, /* %s */" % (addr - anchor, fnv(data), name))
    if carrier:
        # Full native routines, in bounded nofault chunks. Checking just the
        # prologue is insufficient for the newly used private field offsets.
        critical = ['drm_atomic_add_affected_planes', 'drm_atomic_get_plane_state',
                    'drm_atomic_check_only', 'drm_atomic_commit', 'drm_atomic_helper_commit',
                    'drm_atomic_helper_check', 'drm_atomic_helper_check_planes',
                    'drm_atomic_helper_swap_state', 'drm_atomic_helper_commit_planes',
                    'rockchip_drm_atomic_helper_commit_tail_rpm', 'vop2_crtc_atomic_check',
                    'vop2_crtc_atomic_begin', 'vop2_crtc_atomic_flush',
                    'vop2_crtc_duplicate_state', '__drm_atomic_helper_crtc_duplicate_state',
                    'drm_crtc_vblank_count', 'vop2_cfg_done', 'dw_hdmi_qp_bridge_atomic_disable',
                    'regmap_read', 'regmap_write', 'regmap_update_bits_base']
        critical += ['drm_atomic_add_affected_connectors', 'drm_atomic_get_connector_state',
                     'drm_atomic_helper_check_modeset', 'drm_atomic_helper_commit_modeset_disables',
                     'drm_atomic_helper_commit_modeset_enables',
                     'drm_atomic_helper_update_legacy_modeset_state', 'drm_bridge_chain_mode_set',
                     'drm_atomic_bridge_chain_disable', 'drm_atomic_bridge_chain_post_disable',
                     'drm_atomic_bridge_chain_pre_enable', 'drm_atomic_bridge_chain_enable',
                     'vop2_crtc_atomic_enable', 'vop2_crtc_atomic_disable',
                     'dw_hdmi_qp_bridge_atomic_enable', 'dw_hdmi_qp_bridge_mode_set',
                     'drm_crtc_vblank_count_and_time', 'drm_update_vblank_count', '__get_vblank_counter']
        critical += ['dw_hdmi_rockchip_suspend','dw_hdmi_rockchip_resume','dw_hdmi_qp_suspend','dw_hdmi_qp_resume']
        if live_link:
            critical += ['vop2_crtc_standby','vop2_crtc_regs_dump']
            # Same symbol name also exists for legacy HDMI. Only the QP
            # factory register-dump table qualifies diagnostic read offsets.
            diagnostic_regs=(0xaac,0x8e0,0x8e8,0xd08,0xd0c)
            qp_tables=[]
            for table in set(byname.get('hdmi_reg_table',[])):
                rows=struct.unpack_from('<152I',raw,table-BASE)
                ranges=list(zip(rows[::2],rows[1::2]))
                if all(any(first<=reg<=last for first,last in ranges) for reg in diagnostic_regs):
                    qp_tables.append(table)
            require(len(qp_tables)==1,'Ambiguous factory QP diagnostic register table')
            byname['hdmi_qp_diagnostic_reg_table']=qp_tables
            critical += ['hdmi_qp_diagnostic_reg_table']
            critical += ['drm_connector_update_edid_property','drm_property_create_blob',
                         'dw_hdmi_rockchip_bind',
                         'drm_property_lookup_blob','drm_property_blob_put','drm_do_probe_ddc_edid',
                         'dw_hdmi_rk3588_read_hpd','hdptx_post_power_up','hdptx_ropll_cmn_config',
                         'dw_hdmi_rockchip_get_output_bus_format','dw_hdmi_rockchip_get_enc_out_encoding',
                         'drm_crtc_init_with_planes','drm_mode_getcrtc','drm_hdmi_infoframe_set_hdr_metadata',
                         'clk_get_rate','hdptx_phy_clk_recalc_rate']
            # Same symbol names exist for the separate eDP PHY implementation.
            # PIN fixes both the exact address and size of the HDMI variants.
            for name, offset, size in [('rockchip_hdptx_phy_probe',0x609604,1432),
                                       ('rockchip_hdptx_phy_power_on',0x609c54,8844)]:
                require(BASE+offset in byname[name], 'Missing pinned HDMI PHY function: '+name)
                require(min(s['st_value'] for s in symbols if s['st_value']>BASE+offset)-(BASE+offset)==size,
                        'Pinned HDMI PHY function size mismatch: '+name)
                selected=name+'_pinned_hdmi'
                byname[selected]=[BASE+offset];critical.append(selected)
        if functional:
            critical += ['drm_open','drm_release','drm_file_alloc','drm_file_free',
                         'drm_dev_init','drm_dev_get','drm_dev_put','__module_get',
                         'misc_register','misc_open','misc_deregister','vfs_ioctl',
                         'drm_modeset_lock_all','drm_modeset_unlock_all']
        addresses = sorted(set(s['st_value'] for s in symbols if s['st_value'] > BASE))
        jumps = {}
        jump_begin, jump_end = unique('__start___jump_table'), unique('__stop___jump_table')
        require((jump_end-jump_begin) % 16 == 0, 'Native jump table shape mismatch')
        for row in range(jump_begin, jump_end, 16):
            code_delta, target_delta, _ = struct.unpack_from('<iiq', raw, row-BASE)
            site, target = row+code_delta, row+4+target_delta
            require(site % 4 == 0 and target % 4 == 0, 'Unaligned native jump label')
            branch = 0x14000000 | (((target-site)//4) & 0x3ffffff)
            actual, = struct.unpack_from('<I', raw, site-BASE)
            require(actual in (branch, 0xd503201f), 'Jump table opcode mismatch')
            jumps[site] = (actual, 0xd503201f if actual == branch else branch)
        # The pinned Image's VHE alternative switches the per-CPU base read
        # from TPIDR_EL1 to TPIDR_EL2 on this board. Accept ONLY that exact
        # one-instruction replacement certified by its original alt table;
        # do not mask bytes or accept arbitrary observed live code.
        alt_begin,alt_end=unique('__alt_instructions'),unique('__alt_instructions_end')
        require((alt_end-alt_begin)%12==0,'Native alternatives table shape mismatch')
        vhe={}
        for row in range(alt_begin,alt_end,12):
            original_delta,replacement_delta,cap,old_len,new_len=struct.unpack_from('<iiHBB',raw,row-BASE)
            if cap!=11 or old_len!=4 or new_len!=4:continue
            site,replacement=row+original_delta,row+4+replacement_delta
            old,=struct.unpack_from('<I',raw,site-BASE)
            new,=struct.unpack_from('<I',raw,replacement-BASE)
            if (old&~31)==0xd538d080 and (new&~31)==0xd53cd040 and (old&31)==(new&31):
                require(site not in jumps,'Overlapping native patch sites')
                vhe[site]=(old,new)
        patches={**jumps,**vhe}
        for name in critical:
            addr = unique(name)
            end = next(a for a in addresses if a > addr)
            require(end - addr <= 65536, 'Unbounded native function: ' + name)
            start = addr
            while start < end:
                if start in patches:
                    original, alternate = patches[start]
                    kind='VHE TPIDR_EL1/EL2' if start in vhe else 'static-key branch/NOP'
                    lines.append('    { %dL, 4, 0x%016xULL, 0x%016xULL }, /* %s +%d exact %s */' %
                                 (start-anchor, fnv(struct.pack('<I', original)),
                                  fnv(struct.pack('<I', alternate)), name, start-addr,kind))
                    start += 4
                else:
                    finish = min(end, start + 64, min((site for site in patches if start < site < end), default=end))
                    data = raw[start-BASE:finish-BASE]
                    lines.append('    { %dL, %d, 0x%016xULL, 0 }, /* %s +%d */' %
                                 (start-anchor, len(data), fnv(data), name, start-addr))
                    start = finish
    require(fnv(raw[anchor - BASE:anchor - BASE + 32]) == 0x8dc8f550fe138ee9, "printk fingerprint mismatch")
    require(fnv(raw[lookup - BASE:lookup - BASE + 32]) == 0x8476411697e78937, "lookup fingerprint mismatch")
    lines += ["};", "struct display_table_contract { long delta; unsigned int words; u64 relative_mask; u64 slots[32]; };",
              "static const struct display_table_contract display_tables[] = {"]
    tables = [
        ("DRM_DRIVER", "rockchip_drm_driver", 32, None),
        ("MODE_CONFIG", "rockchip_drm_mode_config_funcs", 9, None),
        ("QP_CONNECTOR", "dw_hdmi_connector_funcs", 14, "dw_hdmi_qp_fill_modes"),
        ("QP_CONNECTOR_HELPER", "dw_hdmi_connector_helper_funcs", 10, "dw_hdmi_connector_get_modes"),
        ("QP_BRIDGE", "dw_hdmi_bridge_funcs", 25, "dw_hdmi_qp_bridge_attach"),
        ("CRTC_FUNCS", "vop2_crtc_funcs", 24, None),
        ("CRTC_HELPER", "vop2_crtc_helper_funcs", 16, None),
        ("HDMI_PM", "dw_hdmi_rockchip_pm", 23, None),
    ]
    report = []
    for key, name, words, identity in tables:
        candidates = sorted(set(byname[name]))
        if identity:
            targets = set(byname[identity])
            if key == "QP_CONNECTOR_HELPER":
                # In this SHA-pinned image the QP helper table immediately
                # precedes its connector funcs; names alone are ambiguous.
                candidates = [addr for addr in candidates if addr + 80 == qp_connector_addr]
            else:
                require(len(targets) == 1, "Ambiguous identity: " + identity)
            candidates = [addr for addr in candidates if targets.intersection(struct.unpack_from("<%dQ" % words, raw, addr - BASE))]
        if len(candidates) != 1:
            raise RuntimeError("Cannot identify exact table " + key)
        addr = candidates[0]
        if key == "QP_CONNECTOR":
            qp_connector_addr = addr
        values = struct.unpack_from("<%dQ" % words, raw, addr - BASE)
        normalized = [value - anchor if BASE <= value < BASE + len(raw) else value for value in values]
        mask = sum(1 << i for i, value in enumerate(values) if BASE <= value < BASE + len(raw))
        slots = ", ".join("0x%016xULL" % (value & ((1 << 64) - 1)) for value in normalized)
        lines.append("    { %dL, %d, 0x%xULL, { %s } }, /* %s */" % (addr - anchor, words, mask, slots, key))
        report.append({"key": key, "words_checked": words, "full_table": key != "DRM_DRIVER"})
    lines.append("};")
    for index, (key, _, _, _) in enumerate(tables):
        lines.append("#define DISPLAY_%s %d" % (key, index))
    lines.append("#define DISPLAY_VBLANK_TP_DELTA (%dL)" % (unique("__tracepoint_drm_vblank_event") - anchor))
    for key, symbol in [('VOP_DATA', 'rk3588_vop'), ('REGMAP_LOCK', 'regmap_lock_spinlock'),
                        ('REGMAP_UNLOCK', 'regmap_unlock_spinlock')]:
        lines.append('#define DISPLAY_%s_DELTA (%dL)' % (key, unique(symbol) - anchor))
    if carrier:
        for key, symbol in [('NATIVE_TAIL_HELPERS', 'rockchip_mode_config_helpers'),
                            ('NATIVE_TAIL', 'rockchip_drm_atomic_helper_commit_tail_rpm'),
                            ('NATIVE_FLUSH', 'vop2_crtc_atomic_flush')]:
            lines.append('#define DISPLAY_%s_DELTA (%dL)' % (key, unique(symbol) - anchor))
        for port in range(4):
            lines.append('#define DISPLAY_VP_REGS%d_DELTA (%dL)' %
                         (port,unique('rk3588_vop_vp%d_regs' % port)-anchor))
    if live_link:
        lines.append('#define DISPLAY_DDC_BLOCK_DELTA (%dL)' % (unique('drm_do_probe_ddc_edid')-anchor))
    output.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")
    return report


def main():
    global NAME
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ("ndk", "kernel-source", "image", "elf", "module-template", "work"):
        parser.add_argument("--" + name, type=Path, required=True)
    parser.add_argument("--python-libs", type=Path)
    parser.add_argument("--observer", action="store_true", help="Build passive tracepoint counters instead of object probe")
    parser.add_argument("--qms-abi", action="store_true", help="Read-only native metadata and QMS core compatibility probe")
    parser.add_argument("--qms-carrier", action="store_true", help="Native all-plane transaction check; runtime commit_once=1 additionally performs one identical factory commit")
    parser.add_argument("--qms-control-objects", action="store_true", help="Compile/audit controller and ioctl sources separately; never link/register an incomplete binding")
    parser.add_argument("--qms-live-link", action="store_true", help="Observe native EDID/format/PHY and physical DDC; no display commit, QMS writes or callback publication")
    parser.add_argument("--qms-functional", action="store_true", help="Build restricted cold-bound Decode QMS module with real owner/endpoint; never install or accept hardware automatically")
    parser.add_argument('--qms-owner-inspect',action='store_true',help='Read-only shared owner preparation, no endpoint/publication/commit/BOOT proof')
    parser.add_argument('--qms-owner-sync-inspect',action='store_true',help='Cold factory clock-initialization trial; may commit unchanged native mode; no QMS or publication')
    parser.add_argument('--ddc-repair',action='store_true',help='Exact-image native HDMI segmented EDID repair, reboot-only rollback')
    parser.add_argument('--ddc-recovery',action='store_true',help='Native unchanged-mode reinitialization after corrected EDID; no QMS')
    args = parser.parse_args()
    require(not args.qms_control_objects or args.qms_carrier, 'Control-object audit requires the carrier Image contract')
    require(sum((args.ddc_recovery,args.ddc_repair,args.observer, args.qms_abi, args.qms_carrier, args.qms_live_link,args.qms_functional,args.qms_owner_inspect,args.qms_owner_sync_inspect)) <= 1, 'Select one probe kind')
    owner_build=args.qms_functional or args.qms_owner_inspect or args.qms_owner_sync_inspect
    if args.ddc_recovery:args.qms_carrier=True
    if args.qms_carrier:
        args.qms_abi = True
    if args.observer:
        NAME = "rk_vblank_observer"
    if args.qms_abi:
        require(not args.observer, "Select one probe kind")
        NAME = "rk_qms_abi_probe"
    if args.qms_carrier:
        NAME = "rk_qms_carrier_probe"
    if args.qms_live_link:
        NAME = "rk_qms_link_probe"
    if args.qms_functional:
        NAME = "rk_decode_qms"
    if args.qms_owner_inspect:NAME='rk_qms_owner_probe'
    if args.qms_owner_sync_inspect:NAME='rk_qms_sync_probe'
    if args.ddc_repair:NAME='rk_decode_ddc'
    if args.ddc_recovery:NAME='rk_decode_ddc_recover'
    if args.python_libs:
        sys.path.insert(0, str(args.python_libs.resolve()))
    from elftools.elf.elffile import ELFFile
    # Same-image, already exercised GPL guard supplies only the exact metadata
    # layout, never its executable code. Native do_init_module also loads the
    # init callback at 0x158; delete_module uses exit at 0x360. Pristine Linux
    # headers put these at 0x150/0x348 despite the same total size and vermagic.
    require(sha(args.module_template) == TEMPLATE_SHA, "Module template SHA256 mismatch")
    with args.module_template.open("rb") as stream:
        template = ELFFile(stream)
        section = template.get_section_by_name(".gnu.linkonce.this_module")
        template_data = bytearray(section.data())
        require(len(template_data) == 896 and section["sh_addralign"] == 64, "Module descriptor shape mismatch")
        require(template_data[24:80].split(b"\0")[0] == b"rk_mali_resource_guard", "Template name mismatch")
        template_data[24:80] = bytes(56)
        require(not any(template_data), "Unexpected initialized module metadata")
        relocations = template.get_section_by_name(".rela.gnu.linkonce.this_module")
        symbols = template.get_section(relocations["sh_link"])
        require([(r["r_offset"], symbols.get_symbol(r["r_info_sym"]).name, r["r_addend"])
                for r in relocations.iter_relocations()] == [(0x158, "init_module", 0), (0x360, "cleanup_module", 0)], "Template callback layout mismatch")
    work = args.work.resolve()
    work.mkdir(parents=True, exist_ok=False)
    if args.ddc_repair:
        from ddc_contract import generate
        tables=generate(args.image,args.elf,work/'ddc_contract.h')
    else:
        tables = contract(args.image, args.elf, work / "display_contract.h", args.qms_carrier or args.qms_live_link or owner_build,
                          args.qms_live_link or owner_build,owner_build)
    ks = args.kernel_source.resolve()
    prepared = work / "kbuild"
    shutil.copytree(ROOT / "mali-kernel/prepared", prepared)
    # Kbuild normally generates these forwarding headers. The earlier Mali
    # snapshot lacks wrappers newly needed by this pristine header tree.
    for kbuild in (ks / "include/asm-generic/Kbuild", ks / "arch/arm64/include/asm/Kbuild"):
        for name in re.findall(r"(?:mandatory|generic)-y\s*\+=\s*(\S+\.h)", kbuild.read_text()):
            source = ks / "arch/arm64/include/asm" / name
            generated = prepared / "arch/arm64/include/generated/asm" / name
            if not source.exists() and not generated.exists():
                require((ks / "include/asm-generic" / name).is_file(), "Missing generic header: " + name)
                generated.write_text("#include <asm-generic/%s>\n" % name, newline="\n")
    ndk = args.ndk.resolve()
    host = "windows-x86_64" if os.name == "nt" else "linux-x86_64"
    ext = ".exe" if os.name == "nt" else ""
    llvm = ndk / "toolchains/llvm/prebuilt" / host / "bin"
    paths = {"NDK": ndk, "KERNEL_SOURCE": ks, "COMPAT": ROOT / "mali-kernel/compat"}
    flags = []
    for flag in json.loads((ROOT / "mali-kernel/guard-args.json").read_text()):
        if flag.startswith(("-DMALI_", "-Dkbase", "-DKBUILD_", "-D__KBUILD_", "-fmacro-prefix-map=")) or "@DRIVER@" in flag:
            continue
        for key, value in paths.items():
            flag = flag.replace("@" + key + "@", value.as_posix())
        require("@" not in flag, "Unmapped build flag: " + flag)
        flags.append(flag)
    flags += ['-DKBUILD_MODNAME="' + NAME + '"', '-D__KBUILD_MODNAME=kmod_' + NAME, "-I" + work.as_posix()]
    if args.qms_abi:
        flags += ['-DRKQMS_INSPECT_ABI=1', '-I' + (ROOT / 'decode-kernel/qms').as_posix()]
    if args.qms_carrier:
        flags += ['-DRKQMS_CARRIER=1']
    if args.ddc_recovery:flags+=['-DRK_DECODE_DDC_RECOVERY=1']
    if args.qms_live_link:
        flags += ['-DRKQMS_LIVE_LINK=1', '-I' + (ROOT / 'decode-kernel/qms').as_posix()]
    if owner_build:
        flags += ['-DRKQMS_OWNER=1', '-I' + (ROOT / 'decode-kernel/qms').as_posix()]
    if args.qms_owner_inspect:flags+=['-DRKQMS_OWNER_INSPECT=1']
    if args.qms_owner_sync_inspect:flags+=['-DRKQMS_OWNER_INSPECT=1','-DRKQMS_OWNER_SYNC_INSPECT=1']
    for path, replacement in ((ROOT, "/src"), (work, "/obj"), (ndk, "/ndk")):
        for spelling in (str(path), path.as_posix()):
            flags += ["-ffile-prefix-map=" + spelling + "=" + replacement,
                      "-fmacro-prefix-map=" + spelling + "=" + replacement]

    def run(command):
        result = subprocess.run([str(item) for item in command], cwd=prepared,
                                capture_output=True, text=True, encoding="utf-8", errors="replace", timeout=120)
        if result.returncode:
            raise RuntimeError(result.stdout + result.stderr)
        return result.stdout

    def compile(source, output):
        run([llvm / ("clang" + ext), *flags, '-DKBUILD_BASENAME="display_probe"', "-c", source, "-o", output])

    obj = work / (NAME + ".o")
    source = ROOT / "kernel-probe/display-extension" / ("vblank_observer.c" if args.observer else "probe.c")
    if args.ddc_repair:source=ROOT/'decode-kernel/ddc/repair.c'
    compile(source, obj)
    if owner_build:
        units=['core','edid','transaction','session','lifecycle','controller','endpoint',
               'native_binding','native_link','native_qualify','owner']
        objects=[obj]
        for unit in units:
            target=work/(unit+'.o');compile(ROOT/'decode-kernel/qms'/(unit+'.c'),target);objects.append(target)
        combined=work/'functional-combined.o'
        run([llvm/('ld.lld'+ext),'-r','-o',combined,*objects]);shutil.copyfile(combined,obj)
    if args.qms_control_objects:
        control_objects=[]
        for unit in ('controller','endpoint','lifecycle','native_binding','native_link','native_qualify'):
            target=work/(unit+'.o')
            compile(ROOT/'decode-kernel/qms'/(unit+'.c'),target)
            control_objects.append(target)
        combined=work/'qms-control.o'
        run([llvm/('ld.lld'+ext),'-r','-o',combined,*control_objects])
        control_imports={line.split()[0] for line in run([llvm/('llvm-nm'+ext),'--undefined-only','--format=posix',combined]).splitlines() if line.strip()}
        internal={'__this_module','rkqms_session_fault','rkqms_session_quiesced','rkqms_session_submit','rkqms_session_result','rkqms_session_frame','rkqms_parse_edid'}
        permitted={'__mutex_init','mutex_lock','mutex_trylock','mutex_unlock','_raw_spin_lock_irqsave',
                   '_raw_spin_unlock_irqrestore','kthread_create_on_node','wake_up_process',
                   'kthread_should_stop','kthread_stop','msleep','ktime_get','memcpy',
                   'misc_register','misc_deregister','_copy_from_user','_copy_to_user',
                   '__check_object_size','__stack_chk_fail','__stack_chk_guard',
                   '__arch_copy_from_user','__arch_copy_to_user','arm64_const_caps_ready',
                   'cpu_hwcap_keys','cpu_hwcaps','gic_nonsecure_priorities','gic_pmr_sync','memset'}
        permitted |= LINK_IMPORTS | {'drm_crtc_vblank_get','drm_crtc_vblank_put'}
        require(not (control_imports-internal-permitted),'Unexpected controller/endpoint imports: '+repr(sorted(control_imports-internal-permitted)))
        symvers_control={line.split()[1]:line.split() for line in (ROOT/'mali-kernel/image.Module.symvers').read_text().splitlines()}
        native=sorted(control_imports-internal)
        for name in native:
            require(name in symvers_control and symvers_control[name][2]=='vmlinux','Missing native control export: '+name)
        (work/'CONTROL_BUILD.json').write_text(json.dumps({'status':'SOURCE_OBJECTS_ONLY_NOT_REGISTERED_OR_LINKED_TO_PROBE',
            'sha256':sha(combined),'native_imports':native,'CRCs_available':{name:symvers_control[name][0] for name in native},
            'internal_imports':sorted(control_imports&internal),'functional_native_binding_supplied':False,
            'board_link_validator_and_clean_check_source_supplied':True,
            'source_sha256':{p.relative_to(ROOT).as_posix():sha(p) for p in sorted((ROOT/'decode-kernel/qms').glob('*')) if p.suffix in {'.h','.c'}}},indent=2)+'\n')
    imports = set(run([llvm / ("llvm-nm" + ext), "--undefined-only", "--format=posix", obj]).splitlines())
    imports = {line.split()[0] for line in imports if line.strip()}
    imports.discard("__this_module")  # Defined by standard metadata below.
    approved = OWNER_IMPORTS if owner_build else LINK_IMPORTS if args.qms_live_link else CARRIER_IMPORTS if args.qms_carrier else QMS_ABI_IMPORTS if args.qms_abi else OBSERVER_IMPORTS if args.observer else IMPORTS
    if args.ddc_repair:approved=DDC_IMPORTS
    if not imports or imports - approved:
        raise RuntimeError("Unapproved module imports: " + repr(sorted(imports - approved)))
    symvers = {}
    for line in (ROOT / "mali-kernel/image.Module.symvers").read_text().splitlines():
        fields = line.split()
        symvers[fields[1]] = fields
    required = sorted(imports | {"module_layout"})
    for name in required:
        fields = symvers[name]
        if fields[2] != "vmlinux" or (len(fields) > 4 and fields[4]):
            raise RuntimeError("Unexpected dependency/namespace for " + name)
    modc = work / (NAME + ".mod.c")
    # Standard modversions and exact-image module descriptor; no force-load.
    # Probe C only takes THIS_MODULE's address and never dereferences its layout.
    modc.write_text('''#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/vermagic.h>
BUILD_SALT;
MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);
struct pinned_module_image {
    u8 zero0[24]; char name[56]; u8 zero1[0x158 - 80];
    int (*init)(void); u8 zero2[0x360 - 0x160];
    void (*exit)(void); u8 zero3[896 - 0x368];
} __aligned(64);
static_assert(sizeof(struct pinned_module_image) == 896);
static_assert(offsetof(struct pinned_module_image, init) == 0x158);
static_assert(offsetof(struct pinned_module_image, exit) == 0x360);
__visible struct pinned_module_image pinned_module __asm__("__this_module")
__section(".gnu.linkonce.this_module") = {
    .name = KBUILD_MODNAME,
    .init = init_module,
    .exit = cleanup_module,
};
static const struct modversion_info versions[] __used __section("__versions") = {
''' + "".join('    { %s, "%s" },\n' % (symvers[name][0], name) for name in required) + '''};
MODULE_INFO(depends, "");
''', encoding="utf-8", newline="\n")
    modo = work / (NAME + ".mod.o")
    compile(modc, modo)
    ko = work / (NAME + ".ko")
    run([llvm / ("ld.lld" + ext), "-r", "-EL", "-maarch64elf", "-z", "norelro", "--build-id=sha1",
         "-T", prepared / "module.lds", "-o", ko, obj, modo])
    with ko.open("rb") as stream:
        elf = ELFFile(stream)
        require(elf["e_machine"] == "EM_AARCH64", "Wrong ELF architecture")
        require(elf.get_section_by_name(".gnu.linkonce.this_module")["sh_size"] == 896, "Wrong descriptor size")
        relocations = elf.get_section_by_name(".rela.gnu.linkonce.this_module")
        symbols = elf.get_section(relocations["sh_link"])
        require([(r["r_offset"], symbols.get_symbol(r["r_info_sym"]).name, r["r_addend"])
                for r in relocations.iter_relocations()] == [(0x158, "init_module", 0), (0x360, "cleanup_module", 0)], "Built callback layout mismatch")
        descriptor = bytearray(elf.get_section_by_name(".gnu.linkonce.this_module").data())
        require(descriptor[24:80].split(b"\0")[0] == NAME.encode(), "Built module name mismatch")
        descriptor[24:80] = bytes(56)
        require(not any(descriptor), "Unexpected built metadata data")
        versions = elf.get_section_by_name("__versions").data()
        actual_versions = {}
        for start in range(0, len(versions), 64):
            crc, = struct.unpack_from("<Q", versions, start)
            name = versions[start + 8:start + 64].split(b"\0")[0].decode()
            actual_versions[name] = crc
        require(actual_versions == {name: int(symvers[name][0], 16) for name in required}, "Canonical CRC mismatch")
        metadata = elf.get_section_by_name(".modinfo").data().split(b"\0")
        vermagic = next(item.decode() for item in metadata if item.startswith(b"vermagic="))
        require(vermagic == "vermagic=5.10.66 SMP preempt mod_unload modversions aarch64", "vermagic mismatch")
        for section in elf.iter_sections():
            require(not (section["sh_flags"] & 1 and section["sh_flags"] & 4), "Writable executable section")
        unresolved = {s.name for s in elf.get_section_by_name(".symtab").iter_symbols()
                      if s.name and s["st_shndx"] == "SHN_UNDEF"}
        require(unresolved == imports, "Unresolved import set changed after linking")
    report = {"status": "OFFLINE_BUILD_ONLY_NOT_LOADED", "research_only": True,
              "kind": "live_native_and_DDC_link_qualification_no_display_writes" if args.qms_live_link else "native_all_plane_carrier_probe_optional_factory_commit" if args.qms_carrier else "read_only_qms_abi_probe" if args.qms_abi else "passive_tracepoint_observer" if args.observer else "read_only_object_probe",
              "qms_implemented": False, "ko_sha256": sha(ko), "tables": tables,
              "imports": sorted(imports), "canonical_crcs_checked": len(required),
              "vermagic": vermagic.removeprefix("vermagic="),
              "metadata_generation": "exact-image descriptor offsets independently checked against exercised GPL template; standard modversions",
              "module_init_offset": "0x158", "module_exit_offset": "0x360"}
    if args.qms_functional:
        report.update(kind='restricted_cold_QMS_owner_endpoint_candidate',qms_implemented=True,
                      release_accepted=False,hardware_accepted=False,cold_first_binding_required=True,
                      rollback='disable display_qms and reboot; module remains pinned during this boot',
                      source_sha256={p.relative_to(ROOT).as_posix():sha(p) for p in sorted((ROOT/'decode-kernel/qms').glob('*')) if p.suffix in {'.c','.h'}})
    if args.ddc_repair:
        report.update(kind='native_HDMI_E_DDC_segment_repair',research_only=False,
            release_accepted=False,hardware_accepted=False,display_mode_changes=False,
            EDID_override=False,rollback='disable display_ddc and reboot',
            source_sha256={'decode-kernel/ddc/repair.c':sha(source),'tools/ddc_contract.py':sha(ROOT/'tools/ddc_contract.py')})
    if args.ddc_recovery:
        report.update(kind='native_HDMI_reinitialization_after_EDID_repair',research_only=False,
            release_accepted=False,hardware_accepted=False,QMS_enabled=False,
            native_link_format_renegotiation=True,logical_mode_and_buffers_preserved=True,
            source_sha256={str(p.relative_to(ROOT)).replace('\\','/'):sha(p) for p in
                [source,ROOT/'kernel-probe/display-extension/qms_carrier.h',ROOT/'decode-kernel/qms/native_recover.h']})
    if args.qms_owner_inspect:
        report.update(kind='read_only_shared_owner_preparation_no_publication',qms_implemented=False,
                      endpoint_registration=False,display_commit=False,BOOT_proof=False,
                      source_sha256={p.relative_to(ROOT).as_posix():sha(p) for p in sorted((ROOT/'decode-kernel/qms').glob('*')) if p.suffix in {'.c','.h'}})
    if args.qms_owner_sync_inspect:
        report.update(kind='cold_native_clock_sync_no_QMS_publication',qms_implemented=False,
                      endpoint_registration=False,display_commit='one guarded factory unchanged modeset if zero clock cache',BOOT_proof=False,
                      source_sha256={p.relative_to(ROOT).as_posix():sha(p) for p in sorted((ROOT/'decode-kernel/qms').glob('*')) if p.suffix in {'.c','.h'}})
    (work / "BUILD.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
