/*
 * Copyright (C) 2013 Red Hat
 * Author: Rob Clark <robdclark@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __ETNAVIV_DRM_H__
#define __ETNAVIV_DRM_H__

#define ETNAVIV_DATE_RMK		20130625
#define ETNAVIV_DATE_PENGUTRONIX	20150302
#define ETNAVIV_DATE_PENGUTRONIX2	20150910
#define ETNAVIV_DATE_PENGUTRONIX3	20151126
#define ETNAVIV_DATE_PENGUTRONIX4	20151214
#define ETNAVIV_DATE			ETNAVIV_DATE_PENGUTRONIX

#include <stddef.h>
#include "drm.h"

/* Please note that modifications to all structs defined here are
 * subject to backwards-compatibility constraints:
 *  1) Do not use pointers, use uint64_t instead for 32 bit / 64 bit
 *     user/kernel compatibility
 *  2) Keep fields aligned to their size
 *  3) Because of how drm_ioctl() works, we can add new fields at
 *     the end of an ioctl if some care is taken: drm_ioctl() will
 *     zero out the new fields at the tail of the ioctl, so a zero
 *     value should have a backwards compatible meaning.  And for
 *     output params, userspace won't see the newly added output
 *     fields.. so that has to be somehow ok.
 */

#define ETNADRM_PIPE_3D      0x00
#define ETNADRM_PIPE_2D      0x01
#define ETNADRM_PIPE_VG      0x02

#if ETNAVIV_DATE == ETNAVIV_DATE_PENGUTRONIX
#define ETNA_MAX_PIPES    4
#else
#define ETNA_MAX_PIPES    3
#endif

/* timeouts are specified in clock-monotonic absolute times (to simplify
 * restarting interrupted ioctls).  The following struct is logically the
 * same as 'struct timespec' but 32/64b ABI safe.
 */
struct drm_etnaviv_timespec {
	int64_t tv_sec;          /* seconds */
	int64_t tv_nsec;         /* nanoseconds */
};

#define ETNAVIV_PARAM_GPU_MODEL                     0x01
#define ETNAVIV_PARAM_GPU_REVISION                  0x02
#define ETNAVIV_PARAM_GPU_FEATURES_0                0x03
#define ETNAVIV_PARAM_GPU_FEATURES_1                0x04
#define ETNAVIV_PARAM_GPU_FEATURES_2                0x05
#define ETNAVIV_PARAM_GPU_FEATURES_3                0x06
#define ETNAVIV_PARAM_GPU_FEATURES_4                0x07

#define ETNAVIV_PARAM_GPU_STREAM_COUNT              0x10
#define ETNAVIV_PARAM_GPU_REGISTER_MAX              0x11
#define ETNAVIV_PARAM_GPU_THREAD_COUNT              0x12
#define ETNAVIV_PARAM_GPU_VERTEX_CACHE_SIZE         0x13
#define ETNAVIV_PARAM_GPU_SHADER_CORE_COUNT         0x14
#define ETNAVIV_PARAM_GPU_PIXEL_PIPES               0x15
#define ETNAVIV_PARAM_GPU_VERTEX_OUTPUT_BUFFER_SIZE 0x16
#define ETNAVIV_PARAM_GPU_BUFFER_SIZE               0x17
#define ETNAVIV_PARAM_GPU_INSTRUCTION_COUNT         0x18
#define ETNAVIV_PARAM_GPU_NUM_CONSTANTS             0x19

//#define MSM_PARAM_GMEM_SIZE  0x02

struct drm_etnaviv_param {
	uint32_t pipe;           /* in, ETNA_PIPE_x */
	uint32_t param;          /* in, ETNAVIV_PARAM_x */
	uint64_t value;          /* out (get_param) or in (set_param) */
};

/*
 * GEM buffers:
 */

#define ETNA_BO_CMDSTREAM    0x00000001
#define ETNA_BO_CACHE_MASK   0x000f0000
/* cache modes */
#define ETNA_BO_CACHED       0x00010000
#define ETNA_BO_WC           0x00020000
#define ETNA_BO_UNCACHED     0x00040000
#if ETNAVIV_DATE == ETNAVIV_DATE_PENGUTRONIX
/* map flags */
#define ETNA_BO_FORCE_MMU    0x00100000
#endif

struct drm_etnaviv_gem_new {
	uint64_t size;           /* in */
	uint32_t flags;          /* in, mask of ETNA_BO_x */
	uint32_t handle;         /* out */
};

struct drm_etnaviv_gem_info {
	uint32_t handle;         /* in */
	uint32_t pad;
	uint64_t offset;         /* out, offset to pass to mmap() */
};

#define ETNA_PREP_READ        0x01
#define ETNA_PREP_WRITE       0x02
#define ETNA_PREP_NOSYNC      0x04

struct drm_etnaviv_gem_cpu_prep {
	uint32_t handle;         /* in */
	uint32_t op;             /* in, mask of ETNA_PREP_x */
	struct drm_etnaviv_timespec timeout;   /* in */
};

struct drm_etnaviv_gem_cpu_fini {
	uint32_t handle;         /* in */
};

/*
 * Cmdstream Submission:
 */

/* The value written into the cmdstream is logically:
 *
 *   ((relocbuf->gpuaddr + reloc_offset) << shift) | or
 *
 * When we have GPU's w/ >32bit ptrs, it should be possible to deal
 * with this by emit'ing two reloc entries with appropriate shift
 * values.  Or a new ETNA_SUBMIT_CMD_x type would also be an option.
 *
 * NOTE that reloc's must be sorted by order of increasing submit_offset,
 * otherwise EINVAL.
 */
struct drm_etnaviv_gem_submit_reloc_r20151214 {
	uint32_t submit_offset;  /* in, offset from submit_bo */
	uint32_t reloc_idx;      /* in, index of reloc_bo buffer */
	uint64_t reloc_offset;   /* in, offset from start of reloc_bo */
	uint32_t flags;          /* in, placeholder for now */
};

struct drm_etnaviv_gem_submit_reloc_r20150302 {
	uint32_t submit_offset;  /* in, offset from submit_bo */
	uint32_t reloc_idx;      /* in, index of reloc_bo buffer */
	uint64_t reloc_offset;   /* in, offset from start of reloc_bo */
};

/* Original API */
struct drm_etnaviv_gem_submit_reloc_r20130625 {
	uint32_t submit_offset;  /* in, offset from submit_bo */
	uint32_t or;             /* in, value OR'd with result */
	int32_t  shift;          /* in, amount of left shift (can be negative) */
	uint32_t reloc_idx;      /* in, index of reloc_bo buffer */
	uint64_t reloc_offset;   /* in, offset from start of reloc_bo */
};

/* submit-types:
 *   BUF - this cmd buffer is executed normally.
 *   IB_TARGET_BUF - this cmd buffer is an IB target.  Reloc's are
 *      processed normally, but the kernel does not setup an IB to
 *      this buffer in the first-level ringbuffer
 *   CTX_RESTORE_BUF - only executed if there has been a GPU context
 *      switch since the last SUBMIT ioctl
 */
#define ETNA_SUBMIT_CMD_BUF             0x0001
#if ETNAVIV_DATE == ETNAVIV_DATE_PENGUTRONIX
#define ETNA_SUBMIT_CMD_CTX_RESTORE_BUF 0x0002
#else
#define ETNA_SUBMIT_CMD_IB_TARGET_BUF   0x0002
#define ETNA_SUBMIT_CMD_CTX_RESTORE_BUF 0x0003
#endif
struct drm_etnaviv_gem_submit_cmd_r20150302 {
	uint32_t type;           /* in, one of ETNA_SUBMIT_CMD_x */
	uint32_t submit_idx;     /* in, index of submit_bo cmdstream buffer */
	uint32_t submit_offset;  /* in, offset into submit_bo */
	uint32_t size;           /* in, cmdstream size */
	uint32_t pad;
	uint32_t nr_relocs;      /* in, number of submit_reloc's */
	uint64_t relocs;         /* in, ptr to array of submit_reloc's */
};

struct drm_etnaviv_gem_submit_cmd_r20130625 {
	uint32_t type;           /* in, one of ETNA_SUBMIT_CMD_x */
	uint32_t submit_idx;     /* in, index of submit_bo cmdstream buffer */
	uint32_t submit_offset;  /* in, offset into submit_bo */
	uint32_t size;           /* in, cmdstream size */
	uint32_t pad;
	uint32_t nr_relocs;      /* in, number of submit_reloc's */
	uint64_t relocs;         /* in, ptr to array of submit_reloc's */
};

/* Each buffer referenced elsewhere in the cmdstream submit (ie. the
 * cmdstream buffer(s) themselves or reloc entries) has one (and only
 * one) entry in the submit->bos[] table.
 *
 * As a optimization, the current buffer (gpu virtual address) can be
 * passed back through the 'presumed' field.  If on a subsequent reloc,
 * userspace passes back a 'presumed' address that is still valid,
 * then patching the cmdstream for this entry is skipped.  This can
 * avoid kernel needing to map/access the cmdstream bo in the common
 * case.
 */
#define ETNA_SUBMIT_BO_READ             0x0001
#define ETNA_SUBMIT_BO_WRITE            0x0002
struct drm_etnaviv_gem_submit_bo {
	uint32_t flags;          /* in, mask of ETNA_SUBMIT_BO_x */
	uint32_t handle;         /* in, GEM handle */
	uint64_t presumed;       /* in/out, presumed buffer address */
};

/* Each cmdstream submit consists of a table of buffers involved, and
 * one or more cmdstream buffers.  This allows for conditional execution
 * (context-restore), and IB buffers needed for per tile/bin draw cmds.
 */
struct drm_etnaviv_gem_submit_r20150910 {
	uint32_t fence;          /* out */
	uint32_t pipe;           /* in, ETNA_PIPE_x */
	uint32_t exec_state;     /* in, initial execution state (ETNA_PIPE_x) */
	uint32_t nr_bos;         /* in, number of submit_bo's */
	uint32_t nr_relocs;      /* in, number of submit_reloc's */
	uint32_t stream_size;    /* in, cmdstream size */
	uint64_t bos;            /* in, ptr to array of submit_bo's */
	uint64_t relocs;         /* in, ptr to array of submit_reloc's */
	uint64_t stream;         /* in, ptr to cmdstream */
};

struct drm_etnaviv_gem_submit_r20150302 {
	uint32_t pipe;           /* in, ETNA_PIPE_x */
	uint32_t exec_state;
	uint32_t fence;          /* out */
	uint32_t nr_bos;         /* in, number of submit_bo's */
	uint32_t nr_cmds;        /* in, number of submit_cmd's */
	uint32_t pad;
	uint64_t bos;            /* in, ptr to array of submit_bo's */
	uint64_t cmds;           /* in, ptr to array of submit_cmd's */
};

/* Original submission structure */
struct drm_etnaviv_gem_submit_r20130625 {
	uint32_t pipe;           /* in, ETNA_PIPE_x */
	uint32_t fence;          /* out */
	uint32_t nr_bos;         /* in, number of submit_bo's */
	uint32_t nr_cmds;        /* in, number of submit_cmd's */
	uint64_t bos;            /* in, ptr to array of submit_bo's */
	uint64_t cmds;           /* in, ptr to array of submit_cmd's */
};

/* The normal way to synchronize with the GPU is just to CPU_PREP on
 * a buffer if you need to access it from the CPU (other cmdstream
 * submission from same or other contexts, PAGE_FLIP ioctl, etc, all
 * handle the required synchronization under the hood).  This ioctl
 * mainly just exists as a way to implement the gallium pipe_fence
 * APIs without requiring a dummy bo to synchronize on.
 */
struct drm_etnaviv_wait_fence_r20130625 {
	uint32_t pipe;           /* in, ETNA_PIPE_x */
	uint32_t fence;          /* in */
	struct drm_etnaviv_timespec timeout;   /* in */
};

#define ETNA_WAIT_NONBLOCK      0x01
struct drm_etnaviv_wait_fence_r20151126 {
	__u32 pipe;           /* in */
	__u32 fence;          /* in */
	__u32 flags;          /* in, mask of ETNA_WAIT_x */
	__u32 pad;
	struct drm_etnaviv_timespec timeout;   /* in */
};

#define ETNA_USERPTR_READ       0x01
#define ETNA_USERPTR_WRITE      0x02
struct drm_etnaviv_gem_userptr {
	uint64_t user_ptr;	/* in, page aligned user pointer */
	uint64_t user_size;	/* in, page aligned user size */
	uint32_t flags; 	/* in, flags */
	uint32_t handle;	/* out, non-zero handle */
};

struct drm_etnaviv_gem_wait_r20130625 {
	__u32 pipe;				/* in */
	__u32 handle;				/* in, bo to be waited for */
	struct drm_etnaviv_timespec timeout;	/* in */
};

struct drm_etnaviv_gem_wait_r20151126 {
	__u32 pipe;				/* in */
	__u32 handle;				/* in, bo to be waited for */
	__u32 flags;				/* in, mask of ETNA_WAIT_x  */
	__u32 pad;
	struct drm_etnaviv_timespec timeout;	/* in */
};

#define DRM_ETNAVIV_GET_PARAM          0x00
#define DRM_ETNAVIV_SET_PARAM          0x01
#define DRM_ETNAVIV_GEM_NEW            0x02
#define DRM_ETNAVIV_GEM_INFO           0x03
#define DRM_ETNAVIV_GEM_CPU_PREP       0x04
#define DRM_ETNAVIV_GEM_CPU_FINI       0x05
#define DRM_ETNAVIV_GEM_SUBMIT         0x06
#define DRM_ETNAVIV_WAIT_FENCE         0x07
#define DRM_ETNAVIV_GEM_USERPTR        0x08
#define DRM_ETNAVIV_GEM_WAIT           0x09
#define DRM_ETNAVIV_NUM_IOCTLS         0x0a

#define DRM_IOCTL_ETNAVIV_GET_PARAM    DRM_IOWR(DRM_COMMAND_BASE + DRM_ETNAVIV_GET_PARAM, struct drm_etnaviv_param)
#define DRM_IOCTL_ETNAVIV_GEM_NEW      DRM_IOWR(DRM_COMMAND_BASE + DRM_ETNAVIV_GEM_NEW, struct drm_etnaviv_gem_new)
#define DRM_IOCTL_ETNAVIV_GEM_INFO     DRM_IOWR(DRM_COMMAND_BASE + DRM_ETNAVIV_GEM_INFO, struct drm_etnaviv_gem_info)
#define DRM_IOCTL_ETNAVIV_GEM_CPU_PREP DRM_IOW (DRM_COMMAND_BASE + DRM_ETNAVIV_GEM_CPU_PREP, struct drm_etnaviv_gem_cpu_prep)
#define DRM_IOCTL_ETNAVIV_GEM_CPU_FINI DRM_IOW (DRM_COMMAND_BASE + DRM_ETNAVIV_GEM_CPU_FINI, struct drm_etnaviv_gem_cpu_fini)
#define DRM_IOCTL_ETNAVIV_GEM_SUBMIT   DRM_IOWR(DRM_COMMAND_BASE + DRM_ETNAVIV_GEM_SUBMIT, struct drm_etnaviv_gem_submit)
#define DRM_IOCTL_ETNAVIV_WAIT_FENCE   DRM_IOW (DRM_COMMAND_BASE + DRM_ETNAVIV_WAIT_FENCE, struct drm_etnaviv_wait_fence)
#define DRM_IOCTL_ETNAVIV_GEM_WAIT     DRM_IOW (DRM_COMMAND_BASE + DRM_ETNAVIV_GEM_WAIT, struct drm_etnaviv_gem_wait)

#define __STR(x)			#x
#define _STR(x)				__STR(x)
#define ETNAVIV_DATE_STR		_STR(ETNAVIV_DATE)

#endif /* __ETNAVIV_DRM_H__ */
