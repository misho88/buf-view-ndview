#include <kvnl.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>


char const * KVNL_ERROR_MESSAGES[KVNL_NUMBER_OF_ERRORS] = {
	"success",
	"stream reached EOF",
	"reading from stream failed (consult errno)",
	"malformed specification (contains = or NL)",
	"encoding failed (likely due to a memory error)",
	"writing a size (an integer) failed (likely due to a memory error)",
	"reading needs to either be specified by a terminating character or a size",
	"expected a newline",
};


char const * kvnl_look_up_error(int error)
{
	if (error < 0) error = -error;
	if (error < 0x1000) return "no such error";
	error -= 0x1000;
	if (error > KVNL_NUMBER_OF_ERRORS) return "no such error";
	return KVNL_ERROR_MESSAGES[error - 0x1000];
}


struct kvnl_specification kvnl_specification_from_string(char * key, ssize_t size)
{
	return (struct kvnl_specification){ .key = { key, strlen(key) }, .size = size };
}


ssize_t kvnl_write_some(int fd, struct view view, kvnl_update_func hash)
{
	if (hash != NULL) hash(view);
	return write(fd, view.data, view.size);
}

ssize_t kvnl_write_newline(int fd, kvnl_update_func hash)
{
	return kvnl_write_some(fd, (struct view){ "\n", 1 }, hash);
}


size_t n_digits(size_t n) {
	if (n >= (int)1e19) return 20;
	if (n == 0) return 1;
	for (size_t i = 1, c = 0; ; i *= 10, c++)
		if (i > n) return c;
	return 0;
}


ssize_t kvnl_write_sizes(int fd, ssize_t * sizes, size_t count, kvnl_update_func hash, struct buf * fmt_buf)
{
	struct buf _buf, * buf;
	if (fmt_buf == NULL) {
		_buf = make_buf_grow_only(2.0f);
		buf = &_buf;
	}
	else {
		buf = fmt_buf;
	}

	ssize_t r, total = 0;

	for (size_t d = 0; d < count; d++) {
		ssize_t size = sizes[d];
		size_t s = n_digits(size);

		if (buf_resize(buf, s + 1)) r = -KVNL_WRITE_SIZES_FAILED;
		if (r < 0) goto cleanup;

		ssize_t m = sprintf(buf->data, "%ld%s", size, d == count - 1 ? "" : " ");
		if (m < 0) r = -KVNL_WRITE_SIZES_FAILED;
		if (r < 0) goto cleanup;

		r = kvnl_write_some(fd, (struct view){ buf->data, m }, hash);
		if (r < 0) goto cleanup;
		total += r;
	}
	r = total;
cleanup:
	if (fmt_buf == NULL) buf_free(buf);
	return r;
}

ssize_t kvnl_write_specification(int fd, struct view spec, kvnl_update_func hash)
{
	if (view_equals(spec, view_str("\n"))) return kvnl_write_some(fd, spec, hash);

	if (view_contains(spec, view_str("=")) || view_contains(spec, view_str("\n")))
		return -KVNL_MALFORMED_SPECIFICATION;

	ssize_t m = kvnl_write_some(fd, spec, hash);
	if (m < 0) return m;

	ssize_t n = kvnl_write_some(fd, (struct view){ "=", 1 }, hash);
	if (n < 0) return n;

	return m + n;
}

ssize_t kvnl_write_value(int fd, struct view value, kvnl_update_func hash)
{
	ssize_t m = kvnl_write_some(fd, value, hash);
	if (m < 0) return m;
	ssize_t n = kvnl_write_newline(fd, hash);
	if (n < 0) return n;
	return m + n;
}


ssize_t kvnl_encode_specification(char * key, ssize_t size, struct buf * dest)
{
	int r;
	size_t n = strlen(key);
	if (size < 0) {
		r = buf_resize(dest, n);
		if (r) return r;
		r = view_fill(buf_view(dest), view_str(key));
		if (r) return r;
		return 0;
	}
	size_t s = n_digits(size);
	if (buf_resize(dest, n + s + 2)) return -KVNL_ENCODING_FAILED;
	ssize_t m = sprintf(dest->data, "%s:%lu", key, size);
	if (m < 0) return -KVNL_ENCODING_FAILED;
	if (buf_resize(dest, m)) return -KVNL_ENCODING_FAILED;
	return KVNL_SUCCESS;
}

ssize_t kvnl_write_line(int fd, char * key, struct view value, int sized, kvnl_update_func hash, struct buf * fmt_buf)
{
	struct buf _buf, * buf;
	if (fmt_buf == NULL) {
		_buf = make_buf_grow_only(2.0f);
		buf = &_buf;
	}
	else {
		buf = fmt_buf;
	}
	if (sized < 0) sized = value.size > 1024 || view_contains(value, view_str("\n"));

	ssize_t r, total = 0;
	r = kvnl_encode_specification(key, sized ? (ssize_t)value.size : -1L, buf);
	if (r < 0) goto cleanup;
	r = kvnl_write_specification(fd, buf_view(buf), hash);
	if (r < 0) goto cleanup; else total += r;
	r = kvnl_write_value(fd, value, hash);
	if (r < 0) goto cleanup; else total += r;
	r = total;
cleanup:
	if (fmt_buf == NULL) buf_free(buf);
	return r;
}

ssize_t kvnl_write_ndview(int fd, struct ndview * ndview, char * dtype, size_t item_size, kvnl_update_func hash, struct buf * fmt_buf)
{
	struct buf _buf, * buf;
	if (fmt_buf == NULL) {
		_buf = make_buf_grow_only(2.0f);
		buf = &_buf;
	}
	else {
		buf = fmt_buf;
	}
	ssize_t r, total = 0;
	r = kvnl_write_line(fd, "dtype", view_str(dtype), -1, hash, buf);
	if (r < 0) goto cleanup; else total += r;

	r = kvnl_write_specification(fd, view_str("shape"), hash);
	if (r < 0) goto cleanup; else total += r;
	r = kvnl_write_sizes(fd, (ssize_t *)ndview->shape, ndview->ndim, hash, buf);
	if (r < 0) goto cleanup; else total += r;
	r = kvnl_write_newline(fd, hash);

	r = kvnl_write_specification(fd, view_str("strides"), hash);
	if (r < 0) goto cleanup; else total += r;
	r = kvnl_write_sizes(fd, ndview->strides, ndview->ndim, hash, buf);
	if (r < 0) goto cleanup; else total += r;
	r = kvnl_write_newline(fd, hash);

	struct view value = ndview_memory(ndview, item_size);
	r = kvnl_encode_specification("data", value.size, buf);
	if (r < 0) goto cleanup;
	r = kvnl_write_specification(fd, buf_view(buf), hash);
	if (r < 0) goto cleanup; else total += r;
	r = kvnl_write_value(fd, value, hash);
	if (r < 0) goto cleanup; else total += r;

	r = total;
cleanup:
	if (fmt_buf == NULL) buf_free(buf);
	return r;
}

kvnl_some kvnl_read_some(int fd, ssize_t size, char * delim, struct buf * buf, kvnl_update_func hash)
{
	if (delim == NULL) delim = "";

	if (delim[0] == '\0' && size < 0) {
		return (kvnl_some){ .error = "either the size or some delimiters must be specified" };
	}

	size_t initial_size = buf->size;

	if (delim[0] == '\0') {
		buf_resize(buf, initial_size + size);
		ssize_t n_read = read(fd, buf->data + buf->size - size, size);
		if (n_read < 0) {
			buf_resize(buf, initial_size);
			return (kvnl_some){ .error = "read() failed, consult errno" };
		}
		buf_resize(buf, initial_size + n_read);
		kvnl_some result = {
			.view = { buf->data + initial_size, n_read },
			.error = n_read < size ? "EOF" : NULL
		};
		if (hash != NULL && result.error == NULL) hash(result.view);
		return result;
	}

	const char * error = NULL;
	while (size < 0 || buf->size < initial_size + size) {
		char c;
		ssize_t n_read = read(fd, &c, 1);
		if (n_read <= 0) {
			error = n_read == 0 ? "EOF" : "read() failed, consult errno";
			break;
		}
		buf_append(buf, (struct view){ &c, 1 });
		if (strchr(delim, c) != NULL) /* a delimiter was found */
			break;
	}
	
	kvnl_some result = {
		.view = { buf->data + initial_size, buf->size - initial_size },
		.error = error
	};
	if (hash != NULL && result.error == NULL) hash(result.view);

	return result;
}

kvnl_specification kvnl_read_specification(int fd, struct buf * buf, kvnl_update_func hash)
{
	kvnl_some some = kvnl_read_some(fd, -1, "=\n", buf, hash);
	if (some.error)
		return (kvnl_specification){ .key = some.view, .error = some.error };

	/* we got at least 1 byte of data */
	char * spec = (char *)some.view.data;
	ssize_t n = some.view.size - 1;

	/* if we terminated due to a new line, we're done, but with an error if there was something else read */
	if (spec[n] == '\n')
		return (kvnl_specification){ .key = some.view, .error = n ? "malformed specification" : NULL };

	ssize_t m;
	for (m = n - 1; spec[m] != ':' && m >= 0; m--);

	/* if there was no size specification, return everything but the trailing = */
	if (m < 0)
		return (kvnl_specification){ .key = { spec, n }, .size = -1 };

	char * endptr;
	ssize_t size = strtol(spec + m + 1, &endptr, 10);

	/* if we something went wrong with the size parsing, return the whole spec with an error */
	if (*endptr != '=')
		return (kvnl_specification){ .key = some.view, .error = "malformed size specification" };

	/* return the key name only and the size */
	return (kvnl_specification){ .key = { spec, m }, .size = size };
}

kvnl_line kvnl_read_line(int fd, struct buf * buf, kvnl_update_func hash)
{
	kvnl_specification spec = kvnl_read_specification(fd, buf, hash);
	/* forward any errors */
	if (spec.error)
		return (kvnl_line){ .key = spec.key, .error = spec.error };
	/* empty lines are easy */
	if (spec.key.size == 1 && *(char *)spec.key.data == '\n')
		return (kvnl_line){ .key = spec.key };
	
	/* without a size specification, read until newline */
	if (spec.size < 0) {
		kvnl_some some = kvnl_read_some(fd, -1, "\n", buf, hash);
		struct view value = some.error ? some.view : (struct view){ some.view.data, some.view.size - 1 };
		return (kvnl_line){
			.key = spec.key,
			.size = spec.size,
			.value = value,
			.error = some.error
		};
	}

	kvnl_some some = kvnl_read_some(fd, spec.size, "", buf, hash);
	if (some.error)
		return (kvnl_line){
			.key = spec.key,
			.size = spec.size,
			.value = some.view,
			.error = some.error
		};

	kvnl_some trail = kvnl_read_some(fd, -1, "\n", buf, hash);

	/* if we failed to read the trailing newline */
	if (some.error)
		return (kvnl_line){
			.key = spec.key,
			.size = spec.size,
			.value = trail.view,
			.error = some.error
		};
	
	/* if we read something other than the trailing newline */
	if (trail.view.size != 1)
		return (kvnl_line){
			.key = spec.key,
			.size = spec.size,
			.value = trail.view,
			.error = "expected only a trailing newline"
		};

	return (kvnl_line){
		.key = spec.key,
		.size = spec.size,
		.value = some.view,
	};
}
