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


uint32_t fromRGB(const struct rgb255 *c);
void toRGB(uint32_t c, struct rgb255 *dest);

struct bitmap *bitmap_new(int w, int h);
void bitmap_destroy(struct bitmap *bmap);
struct bitmap *bitmap_clone(struct bitmap *bmap);
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
	struct bitmap *bmap, *copy;

	// FIXME: mostly just testing
	if ((bmap = bitmap_load_ppm(stdin))) {
		if (!(copy = bitmap_clone(bmap))) {
			fprintf(stderr, "ERROR: Could not clone image\n");
			return 0;
		}

		bitmap_togrey(copy);
		bitmap_save_ppm(stdout, copy);

		bitmap_destroy(copy);
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

struct bitmap *bitmap_clone(struct bitmap *bmap)
{
	struct bitmap *bmap_new;

	if (!(bmap_new = bitmap_new(bmap->w, bmap->h)))
		return NULL;

	memcpy(bmap_new->data, bmap->data, bmap->w * bmap->h * sizeof *bmap->data);

	return bmap_new;
}

void bitmap_togrey(struct bitmap *bmap)
{
	int x, y;
	double gsv;

	for (x = 0; x < bmap->w; x++) {
		for (y = 0; y < bmap->h; y++) {
			struct rgb255 rgb;
			uint32_t c = bitmap_getpixel(bmap, x, y);
			toRGB(c, &rgb);
			/* https://en.wikipedia.org/wiki/Grayscale#Colorimetric_.28luminance-preserving.29_conversion_to_grayscale */
			gsv = 0.2126 * rgb.r + 0.7152 * rgb.g + 0.0722 * rgb.b;
			//gsv = round(gsv);
			if (gsv > 255.0) /* paranoia */
				gsv = 255.0;
			rgb.r = gsv = rgb.g = rgb.b = gsv;
			bitmap_setpixel(bmap, fromRGB(&rgb), x, y);
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
 * Misc
 ***************************************************************************/

char *get_line(FILE *fp, char *buff, size_t sz, size_t *linenum)
{
	const char *s;

	do {
		if (!fgets(buff, sz, fp))
			return NULL;

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