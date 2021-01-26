#ifndef __BUF_H__
#define __BUF_H__
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/**
 * buf - simple memory allocation library
 *
 * Buffers are stored as structures (struct buf) with these members:
 *  - data: a pointer to the underlying data,
 *  - size: how much of the data is in use,
 *  - capacity: how much data has been allocated for the buffer.
 *  - under: when size drops below (under * capacity), shrink the allocation
 *  - over: when reallocation, allocate (over * size) bytes
 *  - policy: the policy for exactly how much to allocate
 *
 * The policy can be one of:
 *  - BUF_ALLOC_AUTO: BUF_ALLOC_WORD for sizes less than a page,
 *                    BUF_ALLOC_PAGE for sizes larger than a page
 *  - BUF_ALLOC_WORD: round up the capacity to the nearest word
 *  - BUF_ALLOC_PAGE: round up the capacity to the nearest page
 *  - BUF_ALLOC_EXACT: don't round
 * Regardless of the policy and other parameters, at least 1 byte will be
 *
 * A special immutable buffer NULL_BUF has the first three of these zeroed,
 * and the latter three set to some sensible default values. Starting with:
 * __: struct buf mybuf = NULL_BUF;
 * is not a bad policy, although there are some functions to help with buffer
 * creation described below.
 *
 * Another special buffer INVALID_BUF is used to signal that making the buffer
 * failed.
 *
 * The following functions create buffers (NOTE: they do *not* allocate them):
 *  - make_buf_default(): returns BUFF_NULL
 *  - make_buf_exact(): returns a buffer whose capacity and size will always
 *                      be equal (useful if the buffer is never expected to
 *                      change size)
 *  - make_buf(under, over, policy): create a buffer with the given parameters
 *                                   returns INVALID_BUF if under, over or policy
 *                                   aren't sensible values and sets errno=EINVAL
 *  - make_buf_dynamic(under, over): same as above, but sets policy to BUF_ALLOC_AUTO
 *  - make_buf_grow_only(over): same as above, but sets under to 0.0
 *
 *
 * All other functions take a (struct buf *) or a (struct buf const *) as their
 * first argument, analogously to this or self in OO languages. There is only
 * one complex function, buf_resize(), which is described first.
 *
 * buf_resize(buf, size, overallocation_factor, policy) allocates and resizes
 * the buffer.
 * Arguments:
 *  - buf: pointer to the buffer. Must be in a valid state; see below.
 *  - size: the size of the buffer
 * The first time this function is called on a buffer, it is expected that the
 * buffer will be equal to NULL_BUF (i.e., zeroed out). In this event, it will
 * allocate some memory to it (that is, at least 1 byte, even if the requested
 * size is zero).
 * Subsequent calls may or may not reallocate memory. If the requested size is
 * larger than the current size, but less than the capacity, the size field is
 * simply increased without reallocating. If the requested size is less than
 * (under * capacity), we reallocate to shrink the size.
 *
 * buf_free(buf) frees an allocated buffer
 * Arguments:
 *  - pointer to the buffer to free
 * If buf is null, does nothing, set and returns errno = EINVAL.
 * If buf is invalid, does nothing, set and returns errno = EUCLEAN.
 * If the user thinks freeing is still a good idea in case of an error,
 * free(buf->data) is always an option, but there's probably a bug somewhere.
 *
 *
 * The following compare bufs in certain ways:
 *
 * buf_equal(a, b) compares two buffers
 * It compares only the data, size and capacity fields for equality.
 * The others are somewhat weaker in terms of importance.
 *
 * buf_exactly_equal(a, b) compares two buffers more strictly
 * Every field must be the same.
 *
 * buf_is_null(buf) check if a buf is null
 * same as buf_equal(buf, &NULL_BUF);
 *
 * buf_is_valid(buf) check if a buffer is valid
 * buffers created with make_buf*() pass this check (unless changed later)
 *
 * This prints a buffer:
 * buf_fprint(buf, stream): print some formatted string description of buf to stream
 * buf_print(buf): same, but to stdout
 *
 *
 * Manipulating buffer contents is done through views. A struct view has two
 * fields:
 *  - data: a pointer to some data
 *  - size: the size of the data
 *
 * buf_view(buf): creates a view from buf and returns it. For better or worse
 *                there's not validity check on the buffer here
 *
 */

typedef void * (*realloc_func)(void *, size_t);
typedef void (*free_func)(void *);

enum buf_alloc_policy {
	BUF_ALLOC_AUTO = 0,
	BUF_ALLOC_WORD = 1,
	BUF_ALLOC_PAGE = 2,
	BUF_ALLOC_EXACT = 3,
	BUF_ALLOC_INVALID = 4,
};

struct buf {
	void * data;
	size_t size, capacity;
	float under, over;
	enum buf_alloc_policy policy;
};

extern struct buf const NULL_BUF;
extern struct buf const INVALID_BUF;

struct buf make_buf_default(void);
struct buf make_buf_exact(void);
struct buf make_buf(float under, float over, enum buf_alloc_policy policy);
struct buf make_buf_dynamic(float under, float over);
struct buf make_buf_grow_only(float over);

int buf_resize_with(struct buf *, size_t, realloc_func);
int buf_resize(struct buf *, size_t);
int buf_free_with(struct buf * ptr, free_func);
int buf_free(struct buf * ptr);

int buf_equal(struct buf const *, struct buf const *);
int buf_exactly_equal(struct buf const *, struct buf const *);
int buf_is_null(struct buf const *);
int buf_is_valid(struct buf const *);

int buf_fprint(struct buf const *, FILE *);
int buf_print(struct buf const *);

struct view { void * const data; size_t const size; };

struct view buf_view(struct buf const *);

struct view make_view(void * const data, size_t const size);
int view_fill(struct view, struct view);
int view_copy(struct view, struct view);
struct view view_partial(struct view, size_t, size_t);

#define view_fprint(view_expr, fmt, type, stream) do {\
	size_t stride = sizeof(type), i; \
	struct view v = (view_expr); \
	fprintf(stream, "[ "); \
	for (i = 0; i < v.size - stride; i += stride) \
		fprintf(stream, fmt, *(type *)(v.data + i)); \
	fprintf(stream, fmt " ]", *(type *)(v.data + i)); \
} while (0)
#define view_print(view_expr, fmt, type) do { \
	view_fprint(view_expr, fmt, type, stdout); \
} while (0)

int buf_append(struct buf *, struct view);
#endif//__BUF_H__
