#define CHUMMY_INTERNAL

#include <linux/string.h>
#include <linux/slab.h>

#include "chummy-alloc.h"

#define malloc(a) kmalloc(a, GFP_KERNEL_ACCOUNT)
#define free(a) kfree(a)

#define MIN(a, b) ((a) <= (b) ? (a) : (b))
#define MAX(a, b) ((a) >= (b) ? (a) : (b))

/// child0 is never smaller than child1
#define child0_size(block_psize) (((block_psize) + 1) / 2)
#define child1_size(block_psize) ((block_psize) / 2)

#ifndef chummy_flag_t
#define flag_arg(x)
#else
#define flag_arg(x) , x
#endif

static void block_state_init(chummy_block *block,
			     chummy_frame_num_t max_pfree
				     flag_arg(chummy_flag_t const *flag))
{
	block->max_pfree = max_pfree;

	block->childs[0] = NULL;
	block->childs[1] = NULL;

#ifdef chummy_flag_t
	assert(max_pfree == 0 || flag == NULL);
	block->is_flag_leaf = flag != NULL;
	if (block->is_flag_leaf) {
		memcpy(get_flag(*block), flag, sizeof(chummy_flag_t));
	}
#endif
}
static chummy_block *block_state_new(
	chummy_frame_num_t max_pfree flag_arg(chummy_flag_t const *flag))
{
	chummy_block *block = malloc(sizeof(chummy_block));
	block_state_init(block, max_pfree flag_arg(flag));
	return block;
}
static void block_state_destroy(chummy_block *block)
{
	if (block == NULL) {
		return;
	}

#ifdef chummy_flag_t
	if (!is_ocp_leaf(*block)) {
#endif
		block_state_destroy(get_childs(*block)[0]);
		block_state_destroy(get_childs(*block)[1]);
#ifdef chummy_flag_t
	}
#endif

	free(block);
}

/// assumes childs are accessible
/// assumes that child == NULL means free child
/// at least one child must be non-zero
inline static void block_state_update_max_pfree(chummy_block *block,
						chummy_frame_num_t block_psize)
{
	block->max_pfree = MAX(get_childs(*block)[0] == NULL ?
				       child0_size(block_psize) :
				       get_childs(*block)[0]->max_pfree,
			       get_childs(*block)[1] == NULL ?
				       child1_size(block_psize) :
				       get_childs(*block)[1]->max_pfree);
}
/// assumes childs are accessible
/// assumes that child == NULL means free child
inline static void block_state_coalesc(chummy_block *block,
				       chummy_frame_num_t block_psize)
{
	if ((get_childs(*block)[0] == NULL ? child0_size(block_psize) :
					     get_childs(*block)[0]->max_pfree) +
		    (get_childs(*block)[1] == NULL ?
			     child1_size(block_psize) :
			     get_childs(*block)[1]->max_pfree) ==
	    block_psize) {
		block->max_pfree = block_psize;

		block_state_destroy(get_childs(*block)[0]);
		block_state_destroy(get_childs(*block)[1]);
		get_childs(*block)[0] = NULL;
		get_childs(*block)[1] = NULL;
	} else {
		block_state_update_max_pfree(block, block_psize);
	}
}

static chummy_frame_num_t block_state_palloc(
	chummy_block *block, chummy_frame_num_t block_pstart,
	chummy_frame_num_t block_psize,
	chummy_frame_num_t page_num flag_arg(chummy_flag_t const *flag))
{
	assert(page_num >= 1);
	assert(block_psize >= 1);
	assert(block->max_pfree <= block_psize);

	// this block is not fully occupied, therefore, children are always accessible

	if (page_num > block->max_pfree) {
		return CHUMMY_FRAME_NULL;
	} else if (page_num == block_psize) {
		assert(block->max_pfree == block_psize);
		assert(get_childs(*block)[0] == NULL);
		assert(get_childs(*block)[1] == NULL);

		block->max_pfree = 0;
#ifdef chummy_flag_t
		block->is_flag_leaf = 1;
		memcpy(get_flag(*block), flag, sizeof(chummy_flag_t));
#endif

		return block_pstart;
	} else if (page_num > child0_size(block_psize)) {
		chummy_frame_num_t mid_frn;

		assert(block->max_pfree == block_psize);
		assert(get_childs(*block)[0] == NULL);
		assert(get_childs(*block)[1] == NULL);

		get_childs(*block)[0] = block_state_new(0 flag_arg(flag));

		get_childs(*block)[1] = block_state_new(child1_size(block_psize)
								flag_arg(NULL));
		mid_frn = block_state_palloc(
			get_childs(*block)[1],
			block_pstart + child0_size(block_psize),
			child1_size(block_psize),
			page_num - child0_size(block_psize) flag_arg(flag));
		assert(mid_frn == block_pstart + child0_size(block_psize));

		assert(get_childs(*block)[1]->max_pfree >= 1);
		block->max_pfree = get_childs(*block)[1]->max_pfree;

		return block_pstart;
	} else {
		chummy_frame_num_t frn;

		if (get_childs(*block)[0] == NULL) {
			get_childs(*block)[0] = block_state_new(
				child0_size(block_psize) flag_arg(NULL));
		}
		frn = block_state_palloc(get_childs(*block)[0], block_pstart,
					 child0_size(block_psize),
					 page_num flag_arg(flag));

		if (frn == CHUMMY_FRAME_NULL) {
			if (get_childs(*block)[1] == NULL) {
				get_childs(*block)[1] =
					block_state_new(child1_size(block_psize)
								flag_arg(NULL));
			}
			frn = block_state_palloc(
				get_childs(*block)[1],
				block_pstart + child0_size(block_psize),
				child1_size(block_psize),
				page_num flag_arg(flag));
			assert(frn != CHUMMY_FRAME_NULL);
		}

		block_state_update_max_pfree(block, block_psize);

		if (block->max_pfree == 0) {
#ifndef chummy_flag_t
			assert(get_childs(*get_childs(*block)[0])[0] == NULL);
			assert(get_childs(*get_childs(*block)[0])[1] == NULL);
			assert(get_childs(*get_childs(*block)[1])[0] == NULL);
			assert(get_childs(*get_childs(*block)[1])[1] == NULL);
#endif

#ifdef chummy_flag_t
			if (get_childs(*block)[0]->is_flag_leaf &&
			    get_childs(*block)[1]->is_flag_leaf &&
			    bcmp(get_flag(*get_childs(*block)[0]),
				 get_flag(*get_childs(*block)[1]),
				 sizeof(chummy_flag_t)) == 0) {
				assert(bcmp(flag,
					    get_flag(*get_childs(*block)[0]),
					    sizeof(chummy_flag_t)) == 0);
#endif
				block_state_destroy(get_childs(*block)[0]);
				block_state_destroy(get_childs(*block)[1]);
				block_state_init(block, 0, flag);
#ifdef chummy_flag_t
			}
#endif
		}

		return frn;
	}
}
static int block_state_pfree(
	chummy_block *block, chummy_frame_num_t block_psize,
	chummy_frame_num_t rel_pstart,
	chummy_frame_num_t rel_pend flag_arg(chummy_flag_t const *flag))
{
	int res;

	assert(block_psize >= 1);
	assert(rel_pend <= block_psize);
	assert(rel_pstart < rel_pend);

	if (is_ocp_leaf(*block)) {
#ifndef chummy_flag_t
		assert(get_childs(*block)[0] == NULL);
		assert(get_childs(*block)[1] == NULL);
#endif

#ifdef chummy_flag_t
		if (bcmp(flag, get_flag(*block), sizeof(chummy_flag_t)) != 0) {
			return CHUMMY_EINVFLAG;
		}
#endif

		if (rel_pstart == 0 && rel_pend == block_psize) {
			block->max_pfree = block_psize;

#ifdef chummy_flag_t
			block->is_flag_leaf = 0;
			get_childs(*block)[0] = NULL;
			get_childs(*block)[1] = NULL;
#endif

			return 0;
		} else {
#ifdef chummy_flag_t
			block->is_flag_leaf = 0;
#endif

			// create entirely occupied childs with flag (== get_flag(*block))
			get_childs(*block)[0] =
				block_state_new(0 flag_arg(flag));
			get_childs(*block)[1] =
				block_state_new(0 flag_arg(flag));
		}
	}
	// now NULL childs are always entirely free

	if (rel_pstart < child0_size(block_psize)) {
		if (get_childs(*block)[0] == NULL) {
			// child 0 is already entirely free
			return CHUMMY_EDBLFREE;
		}

		res = block_state_pfree(get_childs(*block)[0],
					child0_size(block_psize), rel_pstart,
					MIN(rel_pend, child0_size(block_psize))
						flag_arg(flag));

		if (res != 0) {
			// error, don't allocate further
			goto coalesc;
		}
	}
	if (rel_pend > child0_size(block_psize)) {
		if (get_childs(*block)[1] == NULL) {
			// child 1 is already entirely free
			return CHUMMY_EDBLFREE;
		}

		res = block_state_pfree(
			get_childs(*block)[1], child1_size(block_psize),
			MAX(rel_pstart, child0_size(block_psize)) -
				child0_size(block_psize),
			rel_pend - child0_size(block_psize) flag_arg(flag));
	}

coalesc:
	// try to coalesc, even if child free partially failed because of double-free
	block_state_coalesc(block, block_psize);

	return res;
}

void chummy_init(chummy_alloc *chummy, chummy_frame_num_t pstart,
		 chummy_frame_num_t pend)
{
	assert(pstart < pend);
	chummy->pstart = pstart;
	chummy->pend = pend;
	block_state_init(&chummy->root, pend - pstart flag_arg(NULL));
}
EXPORT_SYMBOL_GPL(chummy_init);

void chummy_clear(chummy_alloc *chummy)
{
	if (!is_ocp_leaf(chummy->root)) {
		block_state_destroy(get_childs(chummy->root)[0]);
		block_state_destroy(get_childs(chummy->root)[1]);
	}
	block_state_init(&chummy->root,
			 chummy->pend - chummy->pstart flag_arg(NULL));
}
EXPORT_SYMBOL_GPL(chummy_clear);

#ifdef chummy_flag_t
#define chummy_palloc chummy_palloc_flagged
#define chummy_pfree chummy_pfree_flagged
#endif

chummy_frame_num_t
chummy_palloc(chummy_alloc *chummy,
	      chummy_frame_num_t page_num flag_arg(chummy_flag_t const *flag))
{
	if (page_num == 0) {
		return chummy->pend;
	}

	return block_state_palloc(&chummy->root, chummy->pstart,
				  chummy->pend - chummy->pstart,
				  page_num flag_arg(flag));
}
EXPORT_SYMBOL_GPL(chummy_palloc);

int chummy_pfree(chummy_alloc *chummy, chummy_frame_num_t pstart,
		 chummy_frame_num_t page_num flag_arg(chummy_flag_t const *flag))
{
	chummy_frame_num_t pend = pstart + page_num;
	if (pstart > pend || pstart < chummy->pstart ||
	    pstart + page_num > chummy->pend) {
		return CHUMMY_EINVAL;
	} else if (page_num == 0) {
		return 0;
	}
	return block_state_pfree(&chummy->root, chummy->pend - chummy->pstart,
				 pstart - chummy->pstart,
				 pstart + page_num -
					 chummy->pstart flag_arg(flag));
}
EXPORT_SYMBOL_GPL(chummy_pfree);

#ifdef chummy_flag_t

#undef chummy_palloc
#undef chummy_pfree

chummy_frame_num_t chummy_palloc(chummy_alloc *chummy,
				 chummy_frame_num_t page_num)
{
	chummy_flag_t flag;
	memset(&flag, 0, sizeof(chummy_flag_t));

	return chummy_palloc_flagged(chummy, page_num, &flag);
}
EXPORT_SYMBOL_GPL(chummy_palloc);

int chummy_pfree(chummy_alloc *chummy, chummy_frame_num_t pstart,
		 chummy_frame_num_t page_num)
{
	chummy_flag_t flag;
	memset(&flag, 0, sizeof(chummy_flag_t));

	return chummy_pfree_flagged(chummy, pstart, page_num, &flag);
}
EXPORT_SYMBOL_GPL(chummy_pfree);

#endif