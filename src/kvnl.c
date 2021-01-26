#include <kvnl.h>
#include <stdio.h>
#include <limits.h>


char const * KVNL_ERROR_MESSAGES[KVNL_NUMBER_OF_ERRORS] = {
	"success",
	"malformed specification (contains = or NL)",
	"encoding failed (likely due to a memory error)"
};


char const * kvnl_look_up_error(int error)
{
	if (error < 0) error = -error;
	if (error < 0x1000) return "no such error";
	error -= 0x1000;
	if (error > 2) return "no such error";
	return KVNL_ERROR_MESSAGES[error - 0x1000];
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


size_t n_digits(size_t n) {
	if (n >= (int)1e19) return 20;
	if (n == 0) return 1;
	for (size_t i = 1, c = 0; ; i *= 10, c++)
		if (i > n) return c;
	return 0;
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

ssize_t kvnl_write_line(int fd, char * key, struct view value, int sized, kvnl_update_func hash, struct buf * spec_buf)
{
	struct buf buf = spec_buf == NULL ? make_buf_grow_only(2.0f) : *spec_buf;
	if (sized < 0) sized = value.size > 1024 || view_contains(value, view_str("\n"));

	ssize_t r, total = 0;
	r = kvnl_encode_specification(key, sized ? (ssize_t)value.size : -1L, &buf);
	if (r < 0) goto cleanup;
	r = kvnl_write_specification(fd, buf_view(&buf), hash);
	if (r < 0) goto cleanup; else total += r;
	r = kvnl_write_value(fd, value, hash);
	if (r < 0) goto cleanup; else total += r;
	r = total;
cleanup:
	if (spec_buf == NULL) buf_free(&buf);
	return r;
}
