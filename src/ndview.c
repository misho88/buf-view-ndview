#include <ndview.h>

#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

struct ndview INVALID_NDVIEW = { NULL, 0, NULL, NULL };

struct ndview make_ndview(
	void * data,
	size_t ndim,
	size_t * shape,
	ssize_t * strides
)
{
	if (data == NULL) {
		errno = EINVAL;
		return INVALID_NDVIEW;
	}
	return (struct ndview){ data, ndim, shape, strides };
}

struct ndview make_ndview_row_major(
	void * data,
	size_t * shape,
	long * strides,
	char const * spec,
	size_t elem_size
)
{
	if (data == NULL) {
		errno = EINVAL;
		return INVALID_NDVIEW;
	}
	struct ndview view = { data, 0, shape, strides };
	ndview_set_shape(&view, spec);
	ndview_set_strides_row_major(&view, elem_size);
	return view;
}


int ndview_fprint_stats(struct ndview const * view, FILE * stream)
{
	size_t const n = view->ndim;
	if (!n) return fprintf(stream, "<data=%p, shape=[], strides=()>", view->data);

	fprintf(stream, "<data=%p, shape=[%lu", view->data, view->shape[0]);
	for (size_t i = 1; i < n; i++) fprintf(stream, ", %lu", view->shape[i]);
	fprintf(stream, "], strides=(%ld", view->strides[0]);
	for (size_t i = 1; i < n; i++) fprintf(stream, ", %ld", view->strides[i]);
	return fprintf(stream, ")>");
}

int ndview_print_stats(struct ndview const * view)
{
	return ndview_fprint_stats(view, stdout);
}

struct ndview ndview_at(struct ndview const * ndview, size_t idx) {
	if (ndview->ndim == 0 || ndview->shape[0] <= idx) {
		errno = EINVAL;
		return INVALID_NDVIEW;
	}
	return make_ndview(
		ndview->data + idx * ndview->strides[0],
		ndview->ndim - 1,
		ndview->shape + 1,
		ndview->strides + 1
	);
}


void * ndview_vget(struct ndview const * ndview, va_list args) {
	if (ndview->ndim == 0)
		return ndview->data;
	size_t arg = va_arg(args, size_t);
	struct ndview row = ndview_at(ndview, arg);
	return ndview_vget(&row, args);
}

void * ndview_get(struct ndview const * ndview, ...) {
	va_list args;
	va_start(args, ndview);
	void * data = ndview_vget(ndview, args);
	va_end(args);
	return data;
}

int ndview_set_shape(struct ndview * ndview, char const * spec)
{
	size_t spec_len = strlen(spec) + 1;
	char * buff = malloc(spec_len), * mem = buff;
	if (mem == NULL) return errno;

	size_t * shape = ndview->shape;
	ndview->ndim = 0;
	memcpy(buff, spec, spec_len);
	for (char * token; (token = strtok_r(buff, ",", &buff)); shape++) {
		int retval = sscanf(token, "%lu", shape);
		if (retval != 1) {
			errno = EINVAL;
			goto finally;
		}
		ndview->ndim++;
	}
	errno = 0;
finally:
	free(mem);
	return errno;
}

int ndview_set_strides(struct ndview * ndview, char const * spec, size_t elem_size)
{
	size_t spec_len = strlen(spec) + 1;
	char * buff = malloc(spec_len), * mem = buff;
	if (mem == NULL) return errno;

	ssize_t * strides = ndview->strides;
	ndview->ndim = 0;
	memcpy(buff, spec, spec_len);
	for (char * token; (token = strtok_r(buff, ",", &buff)); strides++) {
		int retval = sscanf(token, "%lu", strides);
		if (retval != 1) {
			errno = EINVAL;
			goto finally;
		}
		ndview->ndim++;
		*strides *= elem_size;
	}
	errno = 0;
finally:
	free(mem);
	return errno;
}

int ndview_set_strides_row_major(struct ndview * ndview, size_t elem_size)
{
	size_t ndim = ndview->ndim;
	long stride = elem_size;
	for (long i = ndim - 1; i >= 0; i--) {
		ndview->strides[i] = stride;
		stride *= ndview->shape[i];
	}
	return errno = 0;
}


int ndview_fprint_rec(
	struct ndview const * ndview,
	int (*elem_fprint)(void *, FILE * stream),
	char const * prefix,
	int first,
	FILE * stream
) {
	if (ndview->ndim == 0)
		return elem_fprint(ndview->data, stream);

	if (!first)
		fputs(prefix, stream);
	fputs("[", stream);
	
	size_t prefix_len = strlen(prefix);
	char child_prefix[prefix_len + 2];

	strcpy(child_prefix, prefix);
	strcat(child_prefix, " ");
	int once = 1;

	for ndview_iter(ndview, ax0) {
		ndview_fprint_rec(&ax0.view, elem_fprint, child_prefix, once, stream);
		once = 0;
	}
	fputs(prefix, stream);
	fputs("]", stream);
	fputc('\n', stream);
	return 0;
}

int ndview_fprint(
	struct ndview const * ndview,
	int (*elem_fprint)(void *, FILE * stream),
	FILE * stream
)
{
	return ndview_fprint_rec(ndview, elem_fprint, "", 1, stream);
}

int ndview_is_dense_row_major(struct ndview const * view)
{
	for (long i = view->ndim - 1; i > 0; i--)
		if ((ssize_t)view->shape[i] * view->strides[i] != view->strides[i - 1])
			return 0;
	return 1;
}

struct extent ndview_extent(struct ndview const * ndview, size_t item_size)
{
	struct extent ex = { .lower = 0, .upper = item_size };
	for (size_t d = 0; d < ndview->ndim; ++d) {
		ssize_t addr = (ndview->shape[d] - 1) * ndview->strides[d];
		ex.lower += addr * (ndview->strides[d] < 0);
		ex.upper += addr * (ndview->strides[d] > 0);
	}
	return ex;
}

struct view ndview_memory(struct ndview const * ndview, size_t item_size)
{
	struct extent extent = ndview_extent(ndview, item_size);
	return (struct view){ ndview->data + extent.lower, extent.upper - extent.lower };
}
