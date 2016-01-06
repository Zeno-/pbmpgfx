#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

#define SWAP(type,x,y) do { type temp = (x); (x) = (y); (y) = temp; } while (0)


/***************************************************************************/

struct bitmap {
	unsigned w, h;
	uint32_t *data;
};

enum cmd_id {
	CMD_POINT, CMD_LINE, CMD_RECT, CMD_CIRCLE
};

struct cmd_def {
	const char *str;
	enum cmd_id id;
	int (*fn)(const char *s, struct bitmap *bmap);
};

struct rgb255 {
	unsigned r, g, b;
};

struct point2d {
	unsigned x, y;
};

/***************************************************************************/

int parse_file(FILE *fpi, FILE *fpo);
int parse_cmd_point(const char *s, struct bitmap *bmap);
int parse_cmd_line(const char *s, struct bitmap *bmap);
int parse_cmd_rect(const char *s, struct bitmap *bmap);
int parse_cmd_circle(const char *s, struct bitmap *bmap);

void draw_point(const struct bitmap *bmap, const struct rgb255 *c,
		const struct point2d *p);
void draw_point_xy(const struct bitmap *bmap, const struct rgb255 *c,
		unsigned x, unsigned y);
void draw_line(const struct bitmap *bmap, const struct rgb255 *c,
		const struct point2d *p1, const struct point2d *p2);
void draw_vline(const struct bitmap *bmap, const struct rgb255 *c,
		const struct point2d *p1, const struct point2d *p2);
void draw_hline(const struct bitmap *bmap, const struct rgb255 *c,
		const struct point2d *p1, const struct point2d *p2);
void draw_rect(const struct bitmap *bmap, const struct rgb255 *c,
		const struct point2d *p1, const struct point2d *p2);
void draw_circle(const struct bitmap *bmap, const struct rgb255 *c,
				 const struct point2d *center, unsigned radius);

void bitmap_to_pbmp(FILE *fpo, const struct bitmap *bmap);

void swap_point_ptrs(const struct point2d **p1, const struct point2d **p2);

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
		{ "point",  CMD_POINT,  parse_cmd_point   },
		{ "line",   CMD_LINE,   parse_cmd_line    },
		{ "rect",   CMD_RECT,   parse_cmd_rect    },
		{ "circle", CMD_CIRCLE, parse_cmd_circle  }
	};
	const size_t n_cmds = sizeof cmdlist / sizeof *cmdlist;

	char buff[1024];
	char cmd_s[16];
	struct bitmap bmap;
	int err = 0;

	bmap.data = NULL;
	if (fgets(buff, sizeof buff, fpi)) {
		if (sscanf(buff, "%u %u", &bmap.w, &bmap.h) == 2) {
			if (!(bmap.data = calloc(bmap.w * bmap.h, sizeof *bmap.data)))
				return 1;
		}
	} else {
		return 1;
	}

	while (err == 0 && fgets(buff, sizeof buff, fpi)) {
		if ((sscanf(buff, "%s", cmd_s) == 1)) {
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
	unsigned x, y, w, h;

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
	unsigned r;

	if (sscanf(s, "%u %u %u %u %u %u", &c.r, &c.g, &c.b,
		                               &point.y, &point.x, &r) == 6) {
		draw_circle(bmap, &c, &point, r);
		return 0;
	}
	return 1;
}


/***************************************************************************
 * Drawing
 ***************************************************************************/


void draw_point(const struct bitmap *bmap, const struct rgb255 *c,
				const struct point2d *p)
{
	if (p->x >= bmap->w || p->y >= bmap->h)
		return;

	bmap->data[p->x + p->y * bmap->w] = ( (uint32_t)(c->r & 0xff)) << 16 |
	                                      ((uint32_t)(c->g & 0xff)) << 8 |
	                                      ((uint32_t)(c->b & 0xff) );
}

void draw_point_xy(const struct bitmap *bmap, const struct rgb255 *c,
		unsigned x, unsigned y)
{
	struct point2d point = {x, y};
	draw_point(bmap, c, &point);
}

void draw_line(const struct bitmap *bmap, const struct rgb255 *c,
			   const struct point2d *p1, const struct point2d *p2)
{
	if (p1->x == p2->x)
		draw_vline(bmap, c, p1, p2);
	else if (p1->y == p2->y)
		draw_hline(bmap, c, p1, p2);
	else {		/* Use Bresenham's LDA */
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
		ystep = a.y < b.y ? -1 : 1;

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
	unsigned i;
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
	unsigned i;
	struct point2d p;

	p.y = p1->y;
	if (p1->x > p2->x) swap_point_ptrs(&p1, &p2);

	for (i = p1->x; i <= p2->x; i++) {
		p.x = i;
		draw_point(bmap, c, &p);
	}
}

void draw_rect(const struct bitmap *bmap, const struct rgb255 *c,
			   const struct point2d *p1, const struct point2d *p2)
{
	unsigned y;
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
				 const struct point2d *center, unsigned radius)
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


/***************************************************************************
 * Misc
 ***************************************************************************/

void bitmap_to_pbmp(FILE *fpo, const struct bitmap *bmap)
{
	unsigned row, col;

	fprintf(fpo, "P3 %u %u\n255\n", bmap->w, bmap->h); /* PBMP header */

	for (row = 0; row < bmap->h; row++) {
		for (col = 0; col < bmap->w; col++) {
			uint32_t c = bmap->data[col + row * bmap->w];
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
