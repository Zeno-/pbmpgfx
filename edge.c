#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>
#include <math.h>

struct bitmap {
	int w, h;
	uint32_t *data;
};

struct rgb255 {
	int r, g, b;
};

struct point2d {
	int x, y;
};

uint32_t fromRGB(const struct rgb255 *c);
uint32_t fromRGB_components(uint8_t r, uint8_t g, uint8_t b);
void toRGB(uint32_t c, struct rgb255 *dest);
uint32_t toGrey(uint32_t c);
uint8_t toGrey_8(uint32_t c);


struct bitmap *bitmap_new(int w, int h);
void bitmap_destroy(struct bitmap *bmap);
struct bitmap *bitmap_clone(const struct bitmap *bmap);
struct bitmap *bitmap_edge_sobel(const struct bitmap *bmap);
void bitmap_togrey(struct bitmap *bmap);
void bitmap_setpixel(const struct bitmap *bmap, uint32_t c,
		int x, int y);
uint32_t bitmap_getpixel(const struct bitmap *bmap, int x, int y);

struct bitmap *bitmap_load_ppm(FILE *fp);
void bitmap_save_ppm(FILE *fpo, const struct bitmap *bmap);

char *get_line(FILE *fp, char *buff, size_t sz, size_t *linenum);
const char *skip_leading_spaces(const char *s);

int main(void)
{
	struct bitmap *bmap, *edges;

	if ((bmap = bitmap_load_ppm(stdin))) {
		bitmap_togrey(bmap);
		if ((edges = bitmap_edge_sobel(bmap))) {
			bitmap_save_ppm(stdout, edges);
			bitmap_destroy(edges);
		}
		bitmap_destroy(bmap);

		return 1;
	}

	return 0;
}

/***************************************************************************
 * "Bitmap"
 ***************************************************************************/

struct bitmap *bitmap_new(int w, int h)
{
	struct bitmap *bmap;

	if (w <= 0 || h <= 0)
		return NULL;

	if (!(bmap = malloc(sizeof *bmap)))
		return NULL;

	if (!(bmap->data = calloc(w * h, sizeof *bmap->data))) {
		free(bmap);
		return NULL;
	}

	bmap->w = w;
	bmap->h = h;

	return bmap;
}

void bitmap_destroy(struct bitmap *bmap)
{
	free(bmap->data);
	free(bmap);
}

struct bitmap *bitmap_clone(const struct bitmap *bmap)
{
	struct bitmap *bmap_new;

	if (!(bmap_new = bitmap_new(bmap->w, bmap->h)))
		return NULL;

	memcpy(bmap_new->data, bmap->data, bmap->w * bmap->h * sizeof *bmap->data);

	return bmap_new;
}

/* Stores a 3x3 region with x,y at the center in 'dest'
 * The 24-bit RGB values are converted to greyscale (0-255).
 */
static void sobel_get_3x3region(const struct bitmap *bmap, int x, int y,
		int dest[9])
{
	int dx, dy, x2, y2;
	int idx;

	idx = 0;
	for (dx = -1; dx <= 1; dx++) {
		for (dy = -1; dy <= 1; dy++) {
			x2 = x + dx;
			y2 = y + dx;
			if (x2 < 0 || x2 >= bmap->w || y2 < 0 || y2 >= bmap->w) {
				dest[idx] = 0;
			} else {
				uint8_t gsv = toGrey_8(bitmap_getpixel(bmap, x2, y2));
				dest[idx++] = gsv;
			}
		}
	}
}

/* If 'horiz' == 0 get horizontal gradient, otherwise vertical */
static int sobel_getgradient(const int region[9], int horiz)
{
	static const int sobel_Gx[9] = {
		-1,  0,  1,
		-2,  0,  2,
		-1,  0,  1
	};
	static const int sobel_Gy[9] = {
		-1, -2, -1,
		0,  0,  0,
		1,  2,  1
	};

	const int *K;
	int i;
	int gradient = 0;

	K = horiz ? sobel_Gx : sobel_Gy;
	for (i = 0; i < 9; i++)
		gradient += region[i] * K[i];

	return gradient;
}

struct bitmap *bitmap_edge_sobel(const struct bitmap *bmap)
{
	int region[9];

	struct bitmap *bmap_edges;

	int x, y;
	double gradient_x, gradient_y;

	if (!(bmap_edges = bitmap_new(bmap->w, bmap->h))) {
		fputs("ERROR: (sobel) Could not alloc memory for edge image\n", stderr);
		return NULL;
	}

	for (x = 1; x < bmap->w - 2; x++) {
		for (y = 1; y < bmap->h - 2; y++) {
			struct rgb255 rgb;
			int c;

			sobel_get_3x3region(bmap, x, y, region);
			gradient_x = sobel_getgradient(region, 1);
			gradient_y = sobel_getgradient(region, 0);

			c = sqrt(gradient_x * gradient_x + gradient_y * gradient_y);
			if (c > 255)
				c = 255;
			rgb.r = rgb.g = rgb.b = c;
			bitmap_setpixel(bmap_edges, fromRGB(&rgb), x, y);
		}
	}
	return bmap_edges;

}

void bitmap_togrey(struct bitmap *bmap)
{
	int x, y;
	uint8_t gsv;

	for (x = 0; x < bmap->w; x++) {
		for (y = 0; y < bmap->h; y++) {
			gsv = toGrey_8(bitmap_getpixel(bmap, x, y));
			bitmap_setpixel(bmap, fromRGB_components(gsv, gsv, gsv), x, y);
		}
	}
}

struct bitmap *bitmap_load_ppm(FILE *fp)
{
	char buff[8192];
	size_t line = 0;
	size_t pixel_count = 0;
	int w, h, cc;
	struct bitmap *bmap;
	struct rgb255 rgb;

	get_line(fp, buff, sizeof buff, &line);

	if (strlen(buff) >= 2) {
		if (toupper(buff[0]) != 'P' || buff[1] != '3') {
			fprintf(stderr, "Header incorrect \"%s\"\n", buff);
			return NULL;
		}
	}

	get_line(fp, buff, sizeof buff, &line);
	if (sscanf(buff, "%u %u", &w, &h) != 2) {
		fprintf(stderr, "Load PPM, reading dimensions failed");
		return NULL;
	}

	get_line(fp, buff, sizeof buff, &line);
	if ((sscanf(buff, "%u", &cc) != 1) || cc != 255) {
		fprintf(stderr, "Load PPM, colour format not supported (%d)?\n", cc);
		return NULL;
	}

	if (!(bmap = bitmap_new(w, h)))
		return NULL;

	/* Don't expect comments in the pixel data, so just use scanf() */
	while (scanf("%d %d %d", &rgb.r, &rgb.g, &rgb.b) == 3) {
		bitmap_setpixel(bmap, fromRGB(&rgb),
				pixel_count % w, pixel_count / w);
		pixel_count++;
	}

	if (pixel_count != (size_t)w * h) {
		fprintf(stderr, "Error: %lu pixels read (expected %lu)\n",
			pixel_count, (size_t)w * h);
		bitmap_destroy(bmap);
		return NULL;
	}
	return bmap;

}

void bitmap_save_ppm(FILE *fpo, const struct bitmap *bmap)
{
	int row, col;

	fprintf(fpo, "P3 %u %u\n255\n", bmap->w, bmap->h); /* PBMP header */

	for (row = 0; row < bmap->h; row++) {
		for (col = 0; col < bmap->w; col++) {
			uint32_t c = bitmap_getpixel(bmap, col, row);
			struct rgb255 rgb;
			toRGB(c, &rgb);
			fprintf(fpo, "%-3u %-3u %-3u    ", rgb.r, rgb.g, rgb.b);
		}
		fprintf(fpo, "\n");
	}
}

uint32_t fromRGB(const struct rgb255 *c)
{
	return fromRGB_components(c->r, c->g, c->b);
}

void toRGB(uint32_t c, struct rgb255 *dest)
{
	dest->r = (c >> 16) & 0xff;
	dest->g = (c >> 8) & 0xff;
	dest->b = c & 0xff;
}

uint32_t fromRGB_components(uint8_t r, uint8_t g, uint8_t b)
{
	return
		((uint32_t)(r & 0xff)) << 16 |
		((uint32_t)(g & 0xff)) << 8 |
		((uint32_t)(b & 0xff));
}

uint32_t toGrey(uint32_t c)
{
	uint8_t gsv;

	gsv = toGrey_8(c);

	return fromRGB_components(gsv, gsv, gsv);
}

uint8_t toGrey_8(uint32_t c)
{
	double gsv;
	struct rgb255 rgb;

	toRGB(c, &rgb);
	/* https://en.wikipedia.org/wiki/Grayscale */
	gsv = 0.2126 * rgb.r + 0.7152 * rgb.g + 0.0722 * rgb.b;
	if (gsv > 255)	/* paranoia */
		gsv = 255;

	return gsv;
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
 * Misc
 ***************************************************************************/

char *get_line(FILE *fp, char *buff, size_t sz, size_t *linenum)
{
	const char *s;

	do {
		if (!fgets(buff, sz, fp))
			return NULL;

		if (linenum)
			++*linenum;

		s = skip_leading_spaces(buff);
		if (*s == '\0' || *s == '#')
			continue;	// skip empty lines and comments
		else
			return buff;
	} while (1);

	return NULL;
}

const char *skip_leading_spaces(const char *s)
{
	while (isspace(*s))
		s++;
	return s;
}