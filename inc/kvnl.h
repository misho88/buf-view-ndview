#ifndef __KVNL_H__
#define __KVNL_H__

#include <unistd.h>
#include <buf.h>
#include <ndview.h>

enum KVNL_ERRORS {
	KVNL_SUCCESS = 0x1000,
	KVNL_MALFORMED_SPECIFICATION = 0x1001,
	KVNL_ENCODING_FAILED = 0x1002,
	KVNL_WRITE_SIZES_FAILED = 0x1003
};

#define KVNL_NUMBER_OF_ERRORS 4

extern char const * KVNL_ERROR_MESSAGES[KVNL_NUMBER_OF_ERRORS];
char const * kvnl_look_up_error(int error);

typedef int (*kvnl_update_func)(struct view);

ssize_t kvnl_write_some(int fd, struct view view, kvnl_update_func hash);
ssize_t kvnl_write_newline(int fd, kvnl_update_func hash);
ssize_t kvnl_write_specification(int fd, struct view spec, kvnl_update_func hash);
ssize_t kvnl_write_value(int fd, struct view value, kvnl_update_func hash);
ssize_t kvnl_encode_specification(char * key, ssize_t size, struct buf * dest);
ssize_t kvnl_write_line(int fd, char * key, struct view value, int sized, kvnl_update_func hash, struct buf * spec_buf);
ssize_t kvnl_write_ndview(int fd, struct ndview * ndview, char * dtype, size_t elem_size, kvnl_update_func hash, struct buf * fmt_buf);
#endif//__KVNL_H__
