#include <stdio.h>
#include <buf.h>

#define NL fputc('\n', stdout);

int main()
{
	struct buf buf = make_buf_default();
	buf_append(&buf, (struct view){ "data", 4 });
	buf_fprint(&buf, stdout); NL;
	view_fprint(buf_view(&buf), "%c", char, stdout); NL;
	buf_free(&buf);
}
