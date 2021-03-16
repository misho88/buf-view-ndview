#ifndef __KVNL_H__
#define __KVNL_H__

#include <unistd.h>
#include <buf.h>
#include <ndview.h>

enum KVNL_ERRORS {
	KVNL_SUCCESS = 0x1000,
	KVNL_STREAM_EOF = 0x1001,
	KVNL_STREAM_READ_FAILED = 0x1002,
	KVNL_MALFORMED_SPECIFICATION = 0x1003,
	KVNL_ENCODING_FAILED = 0x1004,
	KVNL_WRITE_SIZES_FAILED = 0x1005,
	KVNL_READ_UNCLEAR_SPECIFICATION = 0x1006,
	KVNL_EXPECTED_NEWLINE = 0x1007,
};

#define KVNL_NUMBER_OF_ERRORS 8

extern char const * KVNL_ERROR_MESSAGES[KVNL_NUMBER_OF_ERRORS];
char const * kvnl_look_up_error(int error);

typedef struct kvnl_some {
	struct view view;
	const char * error;
} kvnl_some;

typedef struct kvnl_specification {
	struct view key;
	ssize_t size;
	const char * error;
} kvnl_specification;

typedef struct kvnl_line {
	struct view key;
	ssize_t size;
	struct view value;
	const char * error;
} kvnl_line;

typedef struct kvnl_block {
	ssize_t n_lines;
	kvnl_line * lines;
} kvnl_block;

#define kvnl_some_fprint(some_expr, fmt, type, stream) do { \
	kvnl_some __some = (some_expr); \
	fputs("kvnl_some { .view = ", stream); \
	view_fprint(__some.view, fmt, type, stream); \
	if (__some.error != NULL) fprintf(stream, ", .error = %s }", __some.error); \
	else                      fputs(" }", stream); \
} while (0)
#define kvnl_some_print(some_expr, fmt, type) do { \
	kvnl_some_fprint(some_expr, fmt, type, stdout); \
} while (0)


#define kvnl_specification_fprint(specification_expr, stream) do { \
	kvnl_specification __specification = (specification_expr); \
	fputs("kvnl_specification { .key = ", stream); \
	view_fprint(__specification.key, "%c", char, stream); \
	if (__specification.size >= 0)     fprintf(stream, ", .size = %ld", __specification.size); \
	if (__specification.error != NULL) fprintf(stream, ", .error = %s }", __specification.error); \
	else                               fputs(" }", stream); \
} while (0)
#define kvnl_specification_print(specification_expr) do { \
	kvnl_specification_fprint(specification_expr, stdout); \
} while (0)


#define kvnl_line_fprint(line_expr, fmt, type, stream) do { \
	kvnl_line __line = (line_expr); \
	fputs("kvnl_line { .key = ", stream); \
	view_fprint(__line.key, "%c", char, stream); \
	if (__line.size >= 0)     fprintf(stream, ", .size = %ld", __line.size); \
	fputs(", .value = ", stream); \
	view_fprint(__line.value, fmt, type, stream); \
	if (__line.error != NULL) fprintf(stream, ", .error = %s }", __line.error); \
	else                      fputs(" }", stream); \
} while (0)
#define kvnl_line_print(specification_expr, fmt, type) do { \
	kvnl_line_fprint(specification_expr, fmt, type, stdout); \
} while (0)



struct kvnl_specification kvnl_decode_specification(struct view spec);

typedef int (*kvnl_update_func)(struct view);

ssize_t kvnl_write_some(int fd, struct view view, kvnl_update_func hash);
ssize_t kvnl_write_newline(int fd, kvnl_update_func hash);
ssize_t kvnl_write_specification(int fd, struct view spec, kvnl_update_func hash);
ssize_t kvnl_write_value(int fd, struct view value, kvnl_update_func hash);
ssize_t kvnl_encode_specification(char * key, ssize_t size, struct buf * dest);
ssize_t kvnl_write_line(int fd, char * key, struct view value, int sized, kvnl_update_func hash, struct buf * spec_buf);
ssize_t kvnl_write_ndview(int fd, struct ndview * ndview, char * dtype, size_t elem_size, kvnl_update_func hash, struct buf * fmt_buf);

kvnl_some kvnl_read_some(int fd, ssize_t size, char * delim, struct buf * buf, kvnl_update_func hash);
kvnl_specification kvnl_read_specification(int fd, struct buf * buf, kvnl_update_func hash);
kvnl_line kvnl_read_line(int fd, struct buf * buf, kvnl_update_func hash);
#endif//__KVNL_H__
