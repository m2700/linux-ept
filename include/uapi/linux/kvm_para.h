/* SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note */
#ifndef _UAPI__LINUX_KVM_PARA_H
#define _UAPI__LINUX_KVM_PARA_H

/*
 * This header file provides a method for making a hypercall to the host
 * Architectures should define:
 * - kvm_hypercall0, kvm_hypercall1...
 * - kvm_arch_para_features
 * - kvm_para_available
 */

/* Return values for hypercalls */
#define KVM_ENOSYS		1000
#define KVM_ENOMEM      ENOMEM
#define KVM_EFAULT		EFAULT
#define KVM_EINVAL		EINVAL
#define KVM_E2BIG		E2BIG
#define KVM_EPERM		EPERM
#define KVM_EOPNOTSUPP		95

#define KVM_HC_VAPIC_POLL_IRQ		1
#define KVM_HC_MMU_OP			2
#define KVM_HC_FEATURES			3
#define KVM_HC_PPC_MAP_MAGIC_PAGE	4
#define KVM_HC_KICK_CPU			5
#define KVM_HC_MIPS_GET_CLOCK_FREQ	6
#define KVM_HC_MIPS_EXIT_VM		7
#define KVM_HC_MIPS_CONSOLE_OUTPUT	8
#define KVM_HC_CLOCK_PAIRING		9
#define KVM_HC_SEND_IPI		10
#define KVM_HC_SCHED_YIELD		11
#define KVM_HC_MAP_GPA_RANGE		12
#define KVM_HC_MAP_EPT_VIEW		100
#define KVM_HC_UNMAP_EPT_VIEW		101
#define KVM_HC_FREEZE_EPT_MAPPING	102
#define KVM_HC_ADD_EPT_ACCESS			103
#define KVM_HC_CREATE_EPT_ACCESS_SET	104
#define KVM_HC_SET_CHUMMY_ALLOCATOR	105
#define KVM_HC_CHUMMY_MALLOC		106
#define KVM_HC_CHUMMY_FREE			107

/*
 * hypercalls use architecture specific
 */
#include <asm/kvm_para.h>

#endif /* _UAPI__LINUX_KVM_PARA_H */
