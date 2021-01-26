#ifndef __NDVIEW_H__
#define __NDVIEW_H__

#include <stdio.h>

struct ndview {
	void * data;
	size_t ndim;
	size_t * shape;
	size_t * strides;
};

extern struct ndview INVALID_NDVIEW;

struct ndview make_ndview(
	void * data,
	size_t ndim,
	size_t * shape,
	long * strides
);
struct ndview make_ndview_row_major(
	void * data,
	size_t * shape,
	long * strides,
	char const * spec,
	size_t elem_size
);

int ndview_set_shape(struct ndview * ndview, char const * spec);
int ndview_set_strides(struct ndview * ndview, char const * spec, size_t elem_size);
int ndview_set_strides_row_major(struct ndview * ndview, size_t elem_size);
//struct view ndview_view(struct ndview const * ndview);

struct ndview ndview_at(struct ndview const * ndview, size_t idx);
void * ndview_get(struct ndview const * ndview, ...);

int ndview_fprint_stats(struct ndview const * view, FILE * stream);
int ndview_print_stats(struct ndview const * view);

struct indexed_ndview {
	size_t index;
	struct ndview view;
};

#define ndview_iter(ndview, name) ( \
	struct indexed_ndview name = { 0 }; \
	name.view = ndview_at((ndview), name.index), name.index < (ndview)->shape[0]; \
	name.index++ \
)

int ndview_fprint(
	struct ndview const * ndview,
	int (*elem_fprint)(void *, FILE * stream),
	FILE * stream
);

#define ndview_declare_print(name, elem_type, fmt, stream) \
	int name(struct ndview const * ndview) { \
		int print_elem(void * data, FILE * stream) { \
			return fprintf(stream, fmt, *(elem_type *)data); \
		} \
		return ndview_fprint(ndview, print_elem, stream); \
	}

#define make_ndview_lazy(data, ndim, spec, dtype) \
	make_ndview_row_major( \
		(data), (size_t[ndim]){}, (long[ndim]){}, \
		(spec), sizeof(dtype) \
	)

int ndview_is_dense_row_major(struct ndview const *);

#endif//__NDVIEW_H__
