#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <math.h>

#define SWAP(type,x,y) do { type temp = (x); (x) = (y); (y) = temp; } while (0)


/***************************************************************************/

struct bitmap {
	int w, h;
	uint32_t *data;
};

enum cmd_id {
	CMD_POINT, CMD_LINE, CMD_RECT, CMD_CIRCLE, CMD_ELLIPSE, CMD_FILL
};

struct cmd_def {
	const char *str;
	enum cmd_id id;
	int (*fn)(const char *s, struct bitmap *bmap);
};

struct rgb255 {
	int r, g, b;
};

struct point2d {
	int x, y;
};

struct point2d_stack {
	size_t max_elems;
	size_t top;
	struct point2d *elems;
};

/***************************************************************************/

int parse_file(FILE *fpi, FILE *fpo);
int parse_cmd_point(const char *s, struct bitmap *bmap);
int parse_cmd_line(const char *s, struct bitmap *bmap);
int parse_cmd_rect(const char *s, struct bitmap *bmap);
int parse_cmd_circle(const char *s, struct bitmap *bmap);
int parse_cmd_ellipse(const char *s, struct bitmap *bmap);
int parse_cmd_fill(const char *s, struct bitmap *bmap);

uint32_t fromRGB(const struct rgb255 *c);
void toRGB(uint32_t c, struct rgb255 *dest);

void bitmap_setpixel(const struct bitmap *bmap, uint32_t c,
		int x, int y);
uint32_t bitmap_getpixel(const struct bitmap *bmap, int x, int y);

void draw_point(const struct bitmap *bmap, const struct rgb255 *c,
		const struct point2d *p);
void draw_point_xy(const struct bitmap *bmap, const struct rgb255 *c,
		int x, int y);

void draw_line(const struct bitmap *bmap, const struct rgb255 *c,
		const struct point2d *p1, const struct point2d *p2);
void draw_vline(const struct bitmap *bmap, const struct rgb255 *c,
		const struct point2d *p1, const struct point2d *p2);
void draw_hline(const struct bitmap *bmap, const struct rgb255 *c,
		const struct point2d *p1, const struct point2d *p2);
void draw_rect(const struct bitmap *bmap, const struct rgb255 *c,
		const struct point2d *p1, const struct point2d *p2);
void draw_circle(const struct bitmap *bmap, const struct rgb255 *c,
		const struct point2d *center, int radius);
void draw_ellipse(const struct bitmap *bmap, const struct rgb255 *c,
		const struct point2d *center,
		int radius1, int radius2);
void draw_fill(const struct bitmap *bmap, const struct rgb255 *c,
		const struct point2d *p);
void draw_fill_scanline(const struct bitmap *bmap, uint32_t fill_colour,
		const struct point2d *p, uint32_t match_colour);

struct point2d_stack *stack_new(size_t sz);
void stack_destroy(struct point2d_stack *stack);
int stack_push(struct point2d_stack *stack, const struct point2d *p);
int stack_pop(struct point2d_stack *stack, struct point2d *p);

void bitmap_to_pbmp(FILE *fpo, const struct bitmap *bmap);

void swap_point_ptrs(const struct point2d **p1, const struct point2d **p2);

const char *skip_leading_spaces(const char *s);

/***************************************************************************/

int main(void)
{
	return parse_file(stdin, stdout) != -1;
}


/***************************************************************************
 * Input parsing
 ***************************************************************************/

int parse_file(FILE *fpi, FILE *fpo)
{
	static const struct cmd_def cmdlist[] = {
		{ "point",   CMD_POINT,  parse_cmd_point   },
		{ "line",    CMD_LINE,   parse_cmd_line    },
		{ "bline",   CMD_LINE,   parse_cmd_line    },
		{ "rect",    CMD_RECT,   parse_cmd_rect    },
		{ "circle",  CMD_CIRCLE, parse_cmd_circle  },
		{ "ellipse", CMD_CIRCLE, parse_cmd_ellipse },
		{ "fill",    CMD_FILL,   parse_cmd_fill    }
	};
	const size_t n_cmds = sizeof cmdlist / sizeof *cmdlist;

	char buff[1024];
	char cmd_s[16];
	struct bitmap bmap;
	int err = 0;
	size_t line = 0;

	bmap.data = NULL;
	do {
		if (fgets(buff, sizeof buff, fpi)) {
			const char *s = skip_leading_spaces(buff);
			line++;
			if (*s == '\0' || *s == '#')
				continue;	// skip empty lines and comments
			if (sscanf(s, "%d %d", &bmap.w, &bmap.h) == 2) {
				if (bmap.w < 0 || bmap.h < 0)
					return 1;
				if (!(bmap.data = calloc(bmap.w * bmap.h, sizeof *bmap.data)))
					return 1;
				break;
			}
		} else {
			return 1;
		}
	} while(1);

	while (err == 0 && fgets(buff, sizeof buff, fpi)) {
		const char *s = skip_leading_spaces(buff);
		line++;
		if (*s == '\0' || *s == '#')
			continue;	// skip empty lines and comments
		if ((sscanf(s, "%s", cmd_s) == 1)) {
			size_t i;
			for (i = 0; i < n_cmds; i++ ) {
				if (strcmp(cmd_s, cmdlist[i].str) == 0) {
					err = cmdlist[i].fn
							? cmdlist[i].fn(buff + strlen(cmd_s), &bmap)
							: 1;
					break;
				}
			}
			if (i == n_cmds)
				err = 1;
		} else {
			err = 1;
		}
		if (err)
			fprintf(stderr, "Syntax error, line %lu: \"%s\"\n", line, s);
	}

	if (err == 0)
		bitmap_to_pbmp(fpo, &bmap);

	free(bmap.data);

	return err;
}

int parse_cmd_point(const char *s, struct bitmap *bmap)
{
	struct rgb255 c;
	struct point2d p;

	if (sscanf(s, "%u %u %u %u %u", &c.r, &c.g, &c.b, &p.y, &p.x) == 5) {
		draw_point(bmap, &c, &p);
		return 0;
	}

	return 1;
}

int parse_cmd_line(const char *s, struct bitmap *bmap)
{
	struct rgb255 c;
	struct point2d p1, p2;

	if (sscanf(s, "%u %u %u %u %u %u %u", &c.r, &c.g, &c.b,
		                                  &p1.y, &p1.x, &p2.y, &p2.x) == 7) {
		draw_line(bmap, &c, &p1, &p2);
		return 0;
	}

	return 1;
}

int parse_cmd_rect(const char *s, struct bitmap *bmap)
{
	struct rgb255 c;
	struct point2d p1, p2;
	int x, y, w, h;

	if (sscanf(s, "%u %u %u %u %u %u %u", &c.r, &c.g, &c.b,
		                                  &y, &x, &h, &w) == 7) {
		p1.x = p2.x = x;
		p1.y = p2.y = y;
		p2.x = x + w;
		p2.y = y + h;

		draw_rect(bmap, &c, &p1, &p2);
		return 0;
	}

	return 1;
}

int parse_cmd_circle(const char *s, struct bitmap *bmap)
{
	struct rgb255 c;
	struct point2d point;
	int r;

	if (sscanf(s, "%u %u %u %u %u %u", &c.r, &c.g, &c.b,
		                               &point.y, &point.x, &r) == 6) {
		draw_circle(bmap, &c, &point, r);
		return 0;
	}
	return 1;
}

int parse_cmd_ellipse(const char *s, struct bitmap *bmap)
{
	struct rgb255 c;
	struct point2d point;
	int r1, r2;

	if (sscanf(s, "%u %u %u %u %u %u %u", &c.r, &c.g, &c.b,
		                               &point.y, &point.x, &r2, &r1) == 7) {
		draw_ellipse(bmap, &c, &point, r1, r2);
		return 0;
	}
	return 1;
}

int parse_cmd_fill(const char *s, struct bitmap *bmap)
{
	struct rgb255 c;
	struct point2d point;

	if (sscanf(s, "%u %u %u %u %u", &c.r, &c.g, &c.b,
		                            &point.y, &point.x) == 5) {
		draw_fill(bmap, &c, &point);
		return 0;
	}
	return 1;
}


/***************************************************************************
 * "Bitmap"
 ***************************************************************************/

uint32_t fromRGB(const struct rgb255 *c)
{
	return
		((uint32_t)(c->r & 0xff)) << 16 |
		((uint32_t)(c->g & 0xff)) << 8 |
		((uint32_t)(c->b & 0xff));
}

void toRGB(uint32_t c, struct rgb255 *dest)
{
	dest->r = (c >> 16) & 0xff;
	dest->g = (c >> 8) & 0xff;
	dest->b = c & 0xff;
}

void bitmap_setpixel(const struct bitmap *bmap, uint32_t c,
		int x, int y)
{
	bmap->data[x + y * bmap->w] = c;
}

uint32_t bitmap_getpixel(const struct bitmap *bmap, int x, int y)
{
	return bmap->data[x + y * bmap->w];
}


/***************************************************************************
 * Drawing
 ***************************************************************************/

void draw_point(const struct bitmap *bmap, const struct rgb255 *c,
				const struct point2d *p)
{
	if (p->x < 0 || p->x >= bmap->w || p->y < 0 || p->y >= bmap->h)
		return;

	bitmap_setpixel(bmap, fromRGB(c), p->x, p->y);
}

void draw_point_xy(const struct bitmap *bmap, const struct rgb255 *c,
		int x, int y)
{
	if (x < 0 || x >= bmap->w || y < 0 || y >= bmap->h)
		return;

	bitmap_setpixel(bmap, fromRGB(c), x, y);
}

void draw_line(const struct bitmap *bmap, const struct rgb255 *c,
			   const struct point2d *p1, const struct point2d *p2)
{
	if (p1->x == p2->x)
		draw_vline(bmap, c, p1, p2);
	else if (p1->y == p2->y)
		draw_hline(bmap, c, p1, p2);
	else {
		/* Use Bresenham's LDA */

		struct point2d a, b;
		int dx, dy, steep, ystep, erracc;
		a.x = p1->x;
		a.y = p1->y;
		b.x = p2->x;
		b.y = p2->y;

		/* steep... 0 shallow-slope, 1 steep */
		steep = abs(b.y - a.y) > abs(b.x - a.x) ? 1 : 0;
		if (steep) { /* "mirror" */
			SWAP(int, a.x, a.y);
			SWAP(int, b.x, b.y);
		}

		if (a.x > b.x)		/* Work along the "x"-axis */
			SWAP(struct point2d, a, b);

		dx = b.x - a.x;
		dy = abs(b.y - a.y);
		ystep = b.y < a.y ? -1 : 1;

		erracc = 2 * dy - dx;

		while (a.x <= b.x) {
			if (steep)
				draw_point_xy(bmap, c, a.y, a.x);
			else
				draw_point(bmap, c, &a);

			if (erracc > 0) {
				a.y += ystep;
				erracc += 2 * dy - 2 * dx;
			} else {
				erracc += 2 * dy;
			}
			a.x++;
		}
	}
}

void draw_vline(const struct bitmap *bmap, const struct rgb255 *c,
				const struct point2d *p1, const struct point2d *p2)
{
	int i;
	struct point2d p;

	p.x = p1->x;
	if (p1->y > p2->y)
		swap_point_ptrs(&p1, &p2);

	for (i = p1->y; i <= p2->y; i++) {
		p.y = i;
		draw_point(bmap, c, &p);
	}
}

void draw_hline(const struct bitmap *bmap, const struct rgb255 *c,
				const struct point2d *p1, const struct point2d *p2)
{
	int i;
	struct point2d p;

	p.y = p1->y;
	if (p1->x > p2->x)
		swap_point_ptrs(&p1, &p2);

	for (i = p1->x; i <= p2->x; i++) {
		p.x = i;
		draw_point(bmap, c, &p);
	}
}

void draw_rect(const struct bitmap *bmap, const struct rgb255 *c,
			   const struct point2d *p1, const struct point2d *p2)
{
	int y;
	struct point2d a, b;

	if (p1->y > p2->y)
		swap_point_ptrs(&p1, &p2);

	for (y = p1->y; y < p2->y; y++) {
		a.x = p1->x; a.y = y;
		b.x = p2->x; b.y = y;
		draw_line(bmap, c, &a, &b);
	}
}

void draw_circle(const struct bitmap *bmap, const struct rgb255 *c,
				 const struct point2d *center, int radius)
{
	int x, y;
	int f;
	int ddFx;
	int ddFy;

	x = 0;
	y = radius;

	ddFx = 1;
	ddFy = -2 * radius;
	f = 1 - radius;

	draw_point_xy(bmap, c, center->x, center->y + radius);
	draw_point_xy(bmap, c, center->x, center->y - radius);
	draw_point_xy(bmap, c, center->x + radius, center->y);
	draw_point_xy(bmap, c, center->x - radius, center->y);

	while (x < y) {
		if (f >= 0) {
			y--;
			ddFy += 2;
			f += ddFy;
		}
		x++;
		ddFx += 2;
		f += ddFx;

		draw_point_xy(bmap, c, center->x + x, center->y + y);
		draw_point_xy(bmap, c, center->x - x, center->y + y);
		draw_point_xy(bmap, c, center->x + x, center->y - y);
		draw_point_xy(bmap, c, center->x - x, center->y - y);

		draw_point_xy(bmap, c, center->x + y, center->y + x);
		draw_point_xy(bmap, c, center->x - y, center->y + x);
		draw_point_xy(bmap, c, center->x + y, center->y - x);
		draw_point_xy(bmap, c, center->x - y, center->y - x);
    }
}

void draw_ellipse(const struct bitmap *bmap, const struct rgb255 *c,
		const struct point2d *center,
		int radius1, int radius2)
{
	/* Adapted from Alois Zingl (2012) "A Rasterizing Algorithm for
	 * Drawing Curves"
	 */
	long x, y, e2, dx, dy, err;

	x = -radius1;
	y = 0;
	e2 = radius2;
	dx = (2 * x + 1) * e2 * e2;
	dy = x * x;
	err = dx + dy;

	do {
		draw_point_xy(bmap, c, center->x - x, center->y + y);
		draw_point_xy(bmap, c, center->x + x, center->y + y);
		draw_point_xy(bmap, c, center->x + x, center->y - y);
		draw_point_xy(bmap, c, center->x - x, center->y - y);

		e2 = 2 * err;
		if (e2 >= dx) {
			x++;
			err += dx += 2 * (long)radius2 * radius2;
		}
		if (e2 <= dy) {
			y++;
			err += dy += 2 * (long)radius1 * radius1;
		}
	} while (x <= 0);

	while (y++ < radius2) {
		draw_point_xy(bmap, c, center->x, center->y + y);
		draw_point_xy(bmap, c, center->x, center->y - y);
	}
}


void draw_fill(const struct bitmap *bmap, const struct rgb255 *c,
		const struct point2d *p)
{
	uint32_t match_colour;
	uint32_t fill_colour;

	if (p->x < 0 || p->x >= bmap->w || p->y < 0 || p->y >= bmap->h)
		return;

	match_colour = bmap->data[p->x + p->y * bmap->w];

	fill_colour = ( (uint32_t)(c->r & 0xff)) << 16 |
	                ((uint32_t)(c->g & 0xff)) << 8 |
	                ((uint32_t)(c->b & 0xff) );

	draw_fill_scanline(bmap, fill_colour, p, match_colour);
}

// pre: coordinates in p are within bitmap bounds
void draw_fill_scanline(const struct bitmap *bmap, uint32_t fill_colour,
		const struct point2d *p, uint32_t match_colour)
{
	int left, right;
	long y;
	struct point2d_stack *stack;
	struct point2d point, point_temp;
	size_t i = 0;

	if ((stack = stack_new(8192)) == NULL) {
		fputs("ERROR: Could not allocate stack for floodfill\n", stderr);
		return;
	}

	if (!stack_push(stack, p)) {
		fputs("Error: floodfill, stack overflow\n", stderr);
		goto abort;
	}

	while (stack_pop(stack, &point)) {
		left = right = 0;
		i++;
		y = point.y;

		while (y >= 0 && bitmap_getpixel(bmap, point.x, y) == match_colour)
			y--;
		y++;

		while (y < bmap->h && bitmap_getpixel(bmap, point.x, y)
						== match_colour) {
			bitmap_setpixel(bmap, fill_colour, point.x, y);

			if (!left && point.x > 0
						&& bitmap_getpixel(bmap, point.x - 1, y)
						== match_colour) {
				point_temp.x = point.x - 1;
				point_temp.y = y;
				if (!stack_push(stack, &point_temp)) {
					fputs("Error: floodfill, stack overflow\n", stderr);
					goto abort;
				}
				left = 1;
			} else if (left && point.x > 0
							&& bitmap_getpixel(bmap, point.x - 1, y)
							!= match_colour) {
				left = 0;
			}

			if (!right && point.x < bmap->w - 1
						&& bitmap_getpixel(bmap, point.x + 1, y)
						== match_colour) {
				point_temp.x = point.x + 1;
				point_temp.y = y;
				if (!stack_push(stack, &point_temp))  {
					fputs("Error: floodfill, stack overflow\n", stderr);
					goto abort;
				}
				right = 1;
			} else if (right && point.x < bmap->w - 1
							&& bitmap_getpixel(bmap, point.x + 1, y)
							!= match_colour) {
				right = 0;
			}

			y++;
		}
	}

abort:
	stack_destroy(stack);
}


/***************************************************************************
 * Point2d Stack
 ***************************************************************************/

struct point2d_stack *stack_new(size_t sz)
{
	struct point2d_stack *stack;

	if (sz == 0 || (stack = malloc(sizeof *stack)) == NULL)
		return NULL;

	if ((stack->elems = malloc(sizeof *stack->elems * sz)) == NULL) {
		free(stack);
		return NULL;
	}

	stack->max_elems = sz;
	stack->top = 0;

	return stack;
}

void stack_destroy(struct point2d_stack *stack)
{
	free(stack->elems);
	free(stack);
}

int stack_push(struct point2d_stack *stack, const struct point2d *p)
{
	if (stack->top + 1 > stack->max_elems)
		return 0;	/* overflow */

	stack->elems[stack->top].x = p->x;
	stack->elems[stack->top].y = p->y;
	stack->top++;

	return 1;
}

int stack_pop(struct point2d_stack *stack, struct point2d *p)
{
	if (stack->top == 0)
		return 0;	/* stack empty */

	p->x = stack->elems[stack->top - 1].x;
	p->y = stack->elems[stack->top - 1].y;
	stack->top--;

	return 1;
}

/***************************************************************************
 * Misc
 ***************************************************************************/

void bitmap_to_pbmp(FILE *fpo, const struct bitmap *bmap)
{
	int row, col;

	fprintf(fpo, "P3 %u %u\n255\n", bmap->w, bmap->h); /* PBMP header */

	for (row = 0; row < bmap->h; row++) {
		for (col = 0; col < bmap->w; col++) {
			uint32_t c = bitmap_getpixel(bmap, col, row);
			fprintf(fpo, "%-3u %-3u %-3u    ", c >> 16, (c >> 8) & 0xff, c & 0xff);
		}
		fprintf(fpo, "\n");
	}
}

void swap_point_ptrs(const struct point2d **p1, const struct point2d **p2)
{
	const struct point2d *temp = *p1;
	*p1 = *p2;
	*p2 = temp;
}

const char *skip_leading_spaces(const char *s)
{
	while (isspace(*s))
		s++;
	return s;
}
