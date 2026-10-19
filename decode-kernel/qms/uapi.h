/* SPDX-License-Identifier: GPL-2.0-only WITH Linux-syscall-note */
#ifndef RKQMS_UAPI_H
#define RKQMS_UAPI_H
#include <linux/types.h>
#include <linux/ioctl.h>

/* Private, versioned Decode Stack interface. No addresses, registers, fences,
 * DRM pointers or arbitrary properties cross this boundary. All reserved
 * fields must be zero. Every ioctl copies one fixed-size object exactly once.
 * SWITCH/RESTORE are blocking: success requires observation of a later real
 * vblank, not just acceptance or completion of a register write.
 */
#define RKQMS_ABI 1U
#define RKQMS_DEVICE "/dev/rk_decode_qms"
#define RKQMS_PROOF_ABI       (1U << 0)
#define RKQMS_PROOF_BOOT      (1U << 1)
#define RKQMS_PROOF_VOP       (1U << 2)
#define RKQMS_PROOF_VTEM      (1U << 3)
#define RKQMS_PROOF_VSYNC     (1U << 4)
#define RKQMS_PROOF_LINK      (1U << 5)
#define RKQMS_PROOF_ALL       0x3fU
#define RKQMS_BASE 0U
#define RKQMS_CHANGING 1U
#define RKQMS_ACTIVE 2U
#define RKQMS_RECOVERY 3U
#define RKQMS_DISCONNECTED 4U
/* Native binding has drained all workers and disabled VTEM. No link required;
 * ordinary native hotplug/power/modeset may run. Not the disconnect IRQ itself.
 */
#define RKQMS_QUIESCENT 5U

struct rkqms_status {
    __u32 abi, bytes;
    __u64 generation, serial, completed_serial;
    __u64 link_cookie, observed_ns;
    __u32 connector, crtc, proof, state;
    __u32 width, height, tfr_mask, base_tfr;
    __u32 effective_tfr, sequence, bits_per_component, colorspace;
    __u32 eotf, reserved0;
    __s32 error;
    __u32 reserved[3];
};

struct rkqms_command {
    __u32 abi, bytes;
    __u64 generation, expected_serial, link_cookie;
    __u32 connector, crtc, target_tfr, timeout_ms;
    __u32 reserved[4];
    struct rkqms_status result;
};

#define RKQMS_GET_STATUS _IOR('Q', 0x70, struct rkqms_status)
#define RKQMS_SWITCH _IOWR('Q', 0x71, struct rkqms_command)
#define RKQMS_RESTORE _IOWR('Q', 0x72, struct rkqms_command)
#endif
