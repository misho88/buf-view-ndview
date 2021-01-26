#include <stdio.h>
#include <buf.h>
#include <kvnl.h>

#define NL fputc('\n', stderr);

#define STREAM 1 /* stdout */

int main()
{
	struct buf buf = make_buf_default();
	buf_append(&buf, (struct view){ "data", 4 });
	//buf_fprint(&buf, stdout); NL;
	//view_fprint(buf_view(&buf), "%c", char, stdout); NL;

	ssize_t r, total = 0;
	r = kvnl_write_line(STREAM, "key", (struct view){ "some data", 9 }, -1, NULL, NULL);
	if (r < 0) {
		fputs(kvnl_look_up_error(r), stderr); NL;
		exit(r);
	}
	total += r;
	r = kvnl_write_line(STREAM, "key", (struct view){ "some\ndata", 9 }, -1, NULL, NULL);
	if (r < 0) {
		fputs(kvnl_look_up_error(r), stderr); NL;
		exit(r);
	}
	total += r;

	fprintf(stderr, "wrote %ld bytes\n", total);
	buf_free(&buf);
}
