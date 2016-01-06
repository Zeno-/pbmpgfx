#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <math.h>

struct bitmap { unsigned w, h; uint32_t *data; };
enum cmd_id { CMD_POINT, CMD_LINE, CMD_RECT };
struct cmd_def { const char *str; enum cmd_id id; int (*fn)(const char *s, struct bitmap *bmap); };
struct rgb255 { unsigned r, g, b; };
struct point2d { unsigned x, y; };

void swap_point_ptrs(const struct point2d **p1, const struct point2d **p2) {
	const struct point2d *temp = *p1; *p1 = *p2; *p2 = temp;
}

void draw_point(const struct bitmap *bmap, const struct rgb255 *c, const struct point2d *p) {
	if (p->x >= bmap->w) return;
	if (p->y >= bmap->h) return;
	bmap->data[p->x + p->y * bmap->w] = ((uint32_t)(c->r & 0xff)) << 16 | ((uint32_t)(c->g & 0xff)) << 8 | ((uint32_t)(c->b & 0xff));
}

void draw_hline(const struct bitmap *bmap, const struct rgb255 *c, const struct point2d *p1, const struct point2d *p2) {
	unsigned i;
	struct point2d p;

	p.y = p1->y;
	if (p1->x > p2->x) swap_point_ptrs(&p1, &p2);
	for (i = p1->x; i <= p2->x; i++) {
		p.x = i;
		draw_point(bmap, c, &p);
	}	
}

void draw_vline(const struct bitmap *bmap, const struct rgb255 *c, const struct point2d *p1, const struct point2d *p2) {
	unsigned i;
	struct point2d p;

	p.x = p1->x;
	if (p1->y > p2->y) swap_point_ptrs(&p1, &p2);
	for (i = p1->y; i <= p2->y; i++) {
		p.y = i;
		draw_point(bmap, c, &p);
	}	
}

void draw_line(const struct bitmap *bmap, const struct rgb255 *c, const struct point2d *p1, const struct point2d *p2) {
	if (p1->x == p2->x) draw_vline(bmap, c, p1, p2);
	else if (p1->y == p2->y) draw_hline(bmap, c, p1, p2);
	else {
		unsigned x;
		double y, dy;
		struct point2d p;
		if (p1->x > p2->x) swap_point_ptrs(&p1, &p2);
		dy = ((double)p1->y - p2->y) / ((double)p1->x - p2->x);
		y = p1->y;
		for (x = p1->x; x <= p2->x; x++) {
			p.x = x; p.y = round(y);
			draw_point(bmap, c, &p);
			y += dy;
		}
	}
}

void draw_rect(const struct bitmap *bmap, const struct rgb255 *c, const struct point2d *p1, const struct point2d *p2) {
	unsigned y;
	struct point2d a, b;
	
	if (p1->y > p2->y) swap_point_ptrs(&p1, &p2);
	for (y = p1->y; y < p2->y; y++) {
		a.x = p1->x; a.y = y;
		b.x = p2->x; b.y = y;
		draw_line(bmap, c, &a, &b); 
	}
}

int parse_cmd_point(const char *s, struct bitmap *bmap) {
	struct rgb255 c;
	struct point2d p;
	
	if (sscanf(s, "%u %u %u %u %u", &c.r, &c.g, &c.b, &p.y, &p.x) == 5) {
		draw_point(bmap, &c, &p);
		return 0;
	}
	return 1;
}

int parse_cmd_line(const char *s, struct bitmap *bmap) {
	struct rgb255 c;
	struct point2d p1, p2;
	
	if (sscanf(s, "%u %u %u %u %u %u %u", &c.r, &c.g, &c.b, &p1.y, &p1.x, &p2.y, &p2.x) == 7) {
		draw_line(bmap, &c, &p1, &p2);
		return 0;
	}
	return 1;
}

int parse_cmd_rect(const char *s, struct bitmap *bmap) {
	struct rgb255 c;
	struct point2d p1, p2;
	unsigned x, y, w, h;
	
	if (sscanf(s, "%u %u %u %u %u %u %u", &c.r, &c.g, &c.b, &y, &x, &h, &w) == 7) {
		p1.x = p2.x = x; p1.y = p2.y = y;
		p2.x = x + w; p2.y = y + h;
		draw_rect(bmap, &c, &p1, &p2);
		return 0;
	}
	return 1;
}

void bitmap_to_pbmp(FILE *fpo, const struct bitmap *bmap) {
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

int parse_file(FILE *fpi, FILE *fpo) {
	static const struct cmd_def cmdlist[] = {
		{"point", CMD_POINT, parse_cmd_point},
		{"line", CMD_LINE, parse_cmd_line}, 
		{"rect", CMD_RECT, parse_cmd_rect} 
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
					err = cmdlist[i].fn ? cmdlist[i].fn(buff + strlen(cmd_s), &bmap) : 1;
					break;
				}
			}
			if (i == n_cmds)
				err = 1;
		} else {
			err = 1;
		}
	}
	
	if (err == 0) bitmap_to_pbmp(fpo, &bmap);
	free(bmap.data);
	return err;
}

int main(void) {
	return parse_file(stdin, stdout) != -1;
}