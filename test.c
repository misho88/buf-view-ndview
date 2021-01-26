#include <stdio.h>
#include <buf.h>
#include <ndview.h>
#include <kvnl.h>

#define NL fputc('\n', stderr);

#define STREAM 1 /* stdout */

int main()
{
	struct buf buf = make_buf_default();
	buf_resize(&buf, 1000 * sizeof(double));

	//struct ndview ndview = make_ndview_row_major(buf.data, (size_t[3]){}, (ssize_t[3]){}, "500, 2", sizeof(double));
	struct ndview ndview = make_ndview_lazy(buf.data, 2, "500, 2", double);
	ndview_fprint_stats(&ndview, stderr); NL;

	ndview_declare_print(ndview_print_double, double, "% 5.3g", stderr);
	ndview_print_double(&ndview);
	//buf_fprint(&buf, stdout); NL;
	//view_fprint(buf_view(&buf), "%c", char, stdout); NL;


	ssize_t r, total = 0;
	r = kvnl_write_line(STREAM, "key", ndview_memory(&ndview, sizeof(double)), -1, NULL, NULL);
	if (r < 0) {
		fputs(kvnl_look_up_error(r), stderr); NL;
		exit(r);
	}
	total += r;

	fprintf(stderr, "wrote %ld bytes\n", total);
	buf_free(&buf);
}
