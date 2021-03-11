#include <buf.h>
#include <errno.h>
#include <unistd.h>
#include <math.h>
#include <stdarg.h>
#include <assert.h>

struct buf const NULL_BUF = {
	.data = NULL,
	.size = 0,
	.capacity = 0,
	.under = 1.0f / 3.0f,
	.over = 1.5f,
	.policy = BUF_ALLOC_AUTO,
};

struct buf const INVALID_BUF = {
	.data = NULL,
	.size = 1,
	.capacity = 0,
	.under = NAN,
	.over = NAN,
	.policy = BUF_ALLOC_INVALID,
};

struct buf make_buf_default(void)
{
	return NULL_BUF;
}
struct buf make_buf_exact(void)
{
	struct buf buf = { NULL, 0, 0, 1.0, 1.0, BUF_ALLOC_EXACT };
	return buf;
}
struct buf make_buf(float under, float over, enum buf_alloc_policy policy)
{
	struct buf buf = { NULL, 0, 0, under, over, policy };
	if (!buf_is_valid(&buf)) { errno = EINVAL; return INVALID_BUF; }
	return buf;
}
struct buf make_buf_dynamic(float under, float over)
{
	return make_buf(under, over, BUF_ALLOC_AUTO);
}
struct buf make_buf_grow_only(float over)
{
	return make_buf_dynamic(0.0, over);
}

int buf_equal(struct buf const * a, struct buf const * b)
{
	return a == b || (
		a->data == b->data &&
		a->size == b->size &&
		a->capacity == b->capacity
	);
}
int buf_exactly_equal(struct buf const * a, struct buf const * b)
{
	return a == b || (
		a->data == b->data &&
		a->size == b->size &&
		a->capacity == b->capacity &&
		a->under == b->under &&
		a->over == b->over &&
		a->policy == b->policy
	);
}
int buf_is_null(struct buf const * buf)
{
	return buf_equal(buf, &NULL_BUF);
}
int buf_is_valid(struct buf const * buf)
{
	if (buf->data == NULL) return buf_is_null(buf);
	return (
		0 < buf->capacity &&
		buf->size <= buf->capacity &&
		0.0f <= buf->under && buf->under <= 1.0 &&
		buf->over >= 1.0f && !isinf(buf->over) &&
		BUF_ALLOC_AUTO <= buf->policy && buf->policy < BUF_ALLOC_INVALID
	);
}

static inline const char * look_up_policy(enum buf_alloc_policy policy)
{
	switch (policy) {
	case BUF_ALLOC_AUTO: return "AUTO";
	case BUF_ALLOC_WORD: return "WORD";
	case BUF_ALLOC_PAGE: return "PAGE";
	case BUF_ALLOC_EXACT: return "EXACT";
	case BUF_ALLOC_INVALID: return "INVALID";
	}
	errno = EINVAL;
	return "bad input to look_up_policy()";
}

int buf_fprint(struct buf const * buf, FILE * stream)
{
	return fprintf(
		stream,
		"{ %p: %lu/%lu ; %.2f-%.1f:%s }",
		buf->data, buf->size, buf->capacity,
		buf->under, buf->over, look_up_policy(buf->policy)
	);
}
int buf_print(struct buf const * buf)
{
	return buf_fprint(buf, stdout);
}

static inline size_t min(size_t a, size_t b) { return a < b ? a : b; }
static inline size_t max(size_t a, size_t b) { return a > b ? a : b; }
static inline size_t round_up(size_t n, size_t r) // n must be a power of two
{
	return ((n - 1) | (r - 1)) + 1;
}

int buf_resize(struct buf * buf, size_t size)
{
	return buf_resize_with(buf, size, realloc);
}

int buf_resize_with(struct buf * buf, size_t size, realloc_func realloc)
{
	// validate the buffer
	if (!buf_is_valid(buf)) return errno = EUCLEAN;

	// decide on how to round the buffer capacity
	size_t const page_size = sysconf(_SC_PAGESIZE);
	size_t const word_size = sizeof(size_t);
	size_t round;
	switch (buf->policy) {
	case BUF_ALLOC_AUTO:
		round = size < page_size ? word_size : page_size;
		break;
	case BUF_ALLOC_WORD:
		round = word_size;
		break;
	case BUF_ALLOC_PAGE:
		round = page_size;
		break;
	case BUF_ALLOC_EXACT:
		round = 1;
		break;
	default:
		return errno = EINVAL;
	}

	// figure out if we need to reallocate
	size_t under_capacity = (size_t)(buf->under * buf->capacity);
	int needs_realloc = buf->capacity < size || size < under_capacity;
	if (!needs_realloc) {
		buf->size = size;
		return errno = 0;
	}

	// (re)allocate; NOTE: realloc(NULL, x) does malloc(x)
	size_t capacity = round_up(max(1, (size_t)(buf->over * size)), round);
	void * data = realloc(buf->data, capacity);
	if (data == NULL) return errno;

	// fill in the buffer
	buf->data = data;
	buf->size = size;
	buf->capacity = capacity;

	return errno = 0;
}

int buf_free(struct buf * buf)
{
	return buf_free_with(buf, free);
}

int buf_free_with(struct buf * buf, free_func free)
{
	if (buf_is_null(buf)) return errno = EINVAL;
	if (!buf_is_valid(buf)) return errno = EUCLEAN;
	free(buf->data);
	*buf = NULL_BUF;
	return errno = 0;
}

struct view view_partial(struct view view, size_t lower, size_t upper)
{
	size_t offset = min(lower, view.size);
	size_t size = min(upper, view.size) - offset;
	struct view new_view = { view.data + offset, size };
	return new_view;
}

struct view buf_view(struct buf const * buf)
{
	struct view view = { buf->data, buf->size };
	return view;
}

struct view make_view(void * const data, size_t const size)
{
	struct view view = { data, size };
	return view;
}

int view_fill(struct view dst, struct view src)
{
	if (src.size == 0 || dst.size % src.size != 0) return errno = EINVAL;

	for (size_t i = 0; i < dst.size; i += src.size)
		memmove(dst.data + i, src.data, src.size);
	return errno = 0;
}

int view_copy(struct view dst, struct view src)
{
	if (dst.size != src.size) return errno = EINVAL;
	return view_fill(dst, src);
}

int buf_append(struct buf * buf, struct view view)
{
	size_t orig_size = buf->size;
	if (buf_resize(buf, orig_size + view.size)) return errno;
	struct view buf_append_view = view_partial(buf_view(buf), orig_size, buf->size);
	return view_copy(buf_append_view, view);
}

struct view view_str(char * str) { return (struct view){ str, strlen(str) }; }

int view_equals(struct view view, struct view other)
{
	if (view.size != other.size) return 0;
	return memcmp(view.data, other.data, view.size) == 0;
}

int view_contains(struct view view, struct view other)
{
	ssize_t end = (ssize_t)view.size - (ssize_t)other.size;
	for (ssize_t i = 0; i <= end; ++i)
		if (memcmp(view.data + i, other.data, other.size) == 0)
			return 1;
	return 0;
}

struct view view_difference(struct view a, struct view b)
{
	void * a_end = a.data + a.size, * b_end = b.data + b.size;
	/* no intersection */
	if (a_end < b.data || b_end < a.data) return a;
	/* intersection at start */
	if (a.data >= b.data) return (struct view){ b.data, a_end - b.data };
	/* intersection at end */
	return (struct view){ a.data, b.data - a.data };
}

int buf_printf_into(struct buf * buf, char const * fmt, ...)
{
	va_list args1, args2;
	va_start(args1, fmt);
	va_copy(args2, args1);

	int retval;

	if (buf_is_null(buf)) { if ((retval = buf_resize(buf, 0))) goto finish; }

	int n = vsnprintf(buf->data, buf->capacity, fmt, args1);
	if (n < 0) {
		retval = n;
		goto finish;
	};

	if ((size_t)n + 1 <= buf->capacity) {
		buf->size = n + 1;
		retval = n;
		goto finish;
	}

	if ((retval = buf_resize(buf, n + 1))) goto finish;
	n = vsnprintf(buf->data, buf->size, fmt, args2);
	if (n < 0) {
		retval = n;
		goto finish;
	};
	assert((size_t)n + 1 == buf->size);
	retval = n;

finish:
	va_end(args1);
	va_end(args2);
	return retval;
}
