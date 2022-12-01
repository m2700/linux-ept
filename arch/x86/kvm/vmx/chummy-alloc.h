#pragma once

#include <linux/kvm_types.h>

#define chummy_frame_num_t gfn_t
#define chummy_flag_t size_t

#define CHUMMY_FRAME_NULL (chummy_frame_num_t) - 1

/// invalid input value
#define CHUMMY_EINVAL -KVM_EINVAL
/// double free
#define CHUMMY_EDBLFREE -KVM_EFAULT

#ifdef chummy_flag_t
/// wrong free flag
#define CHUMMY_EINVFLAG -KVM_EPERM
#endif

typedef struct chummy_block chummy_block;
struct chummy_block {
	chummy_frame_num_t max_pfree;
#ifdef chummy_flag_t
	/// @brief only relevant for max_pfree == 0
	char is_flag_leaf;
	union {
#endif
		chummy_block *childs[2];
#ifdef chummy_flag_t
		chummy_flag_t flag;
	};
#endif
};

typedef struct chummy_alloc {
	chummy_frame_num_t pstart;
	chummy_frame_num_t pend;
	chummy_block root;
} chummy_alloc;

void chummy_init(chummy_alloc *chummy, chummy_frame_num_t pstart,
		 chummy_frame_num_t pend);
/// deallocates all internal metadata
void chummy_clear(chummy_alloc *chummy);

chummy_frame_num_t chummy_palloc(chummy_alloc *chummy,
				 chummy_frame_num_t page_num);
#ifdef chummy_flag_t
chummy_frame_num_t chummy_palloc_flagged(chummy_alloc *chummy,
					 chummy_frame_num_t page_num,
					 chummy_flag_t const *flag);
#endif

int chummy_pfree(chummy_alloc *chummy, chummy_frame_num_t pstart,
		 chummy_frame_num_t page_num);
#ifdef chummy_flag_t
int chummy_pfree_flagged(chummy_alloc *chummy, chummy_frame_num_t pstart,
			 chummy_frame_num_t page_num,
			 chummy_flag_t const *flag);
#endif

#ifdef CHUMMY_INTERNAL

/// child0 is never smaller than child1
#define child0_size(block_psize) (((block_psize) + 1) / 2)
#define child1_size(block_psize) ((block_psize) / 2)

// this is copied code!!!
#ifndef chummy_flag_t
#define flag_arg(x)
#else
#define flag_arg(x) , x
#endif

#ifndef chummy_flag_t

#define is_ocp_leaf(block) ((block).max_pfree == 0)
#define get_childs(block) (block).childs

#else // chummy_flag_t == void

#define is_ocp_leaf(block)                                 \
	({                                                 \
		if ((block).is_flag_leaf) {                \
			assert((block).is_flag_leaf == 1); \
			assert((block).max_pfree == 0);    \
		}                                          \
		(block).is_flag_leaf;                      \
	})
#define get_childs(block)                    \
	({                                   \
		assert(!is_ocp_leaf(block)); \
		(block).childs;              \
	})
#define get_flag(block)                     \
	({                                  \
		assert(is_ocp_leaf(block)); \
		&(block).flag;              \
	})

#endif // chummy_flag_t == void

#endif // CHUMMY_INTERNAL