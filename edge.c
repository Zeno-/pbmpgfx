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
void toRGB(uint32_t c, struct rgb255 *dest);

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

	// FIXME: mostly just testing
	if ((bmap = bitmap_load_ppm(stdin))) {

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

/* param gethoriz: 0 = sample vertically, 1 = sample horizontally */
static void get_sobel_samples(const struct bitmap *bmap, int x, int y,
		uint32_t samples[6], int gethoriz)
{
	/* x, y offsets (sample locations) for horizontal calc */
	static const struct point2d sample_offsets_h[6] = {
		{  1, -1}, {  1,  0 }, {  1, 1 },
		{ -1, -1}, { -1,  0 }, { -1, 1 }
	};
	/* x, y offsets (sample locations) for vertical calc */
	static const struct point2d sample_offsets_v[6] = {
		{ -1,  1}, {  0,  1 }, {  1,  1 },
		{ -1, -1}, {  0, -1 }, {  1, -1 }
	};

	int x2, y2, si, brightness;
	const struct point2d *M;

	M = gethoriz ? sample_offsets_h : sample_offsets_v;

	for (si = 0; si < 6; si++) {
		x2 = x + M[si].x;
		y2 = y + M[si].y;
		if (x2 > 0 && x2 < bmap->w && y2 > 0 && y2 < bmap->h) {
			struct rgb255 rgb;
			toRGB(bitmap_getpixel(bmap, x2, y2), &rgb);
			//brightness = (rgb.r + rgb.g + rgb.b) / 3;
			brightness = 0.2126 * rgb.r + 0.7152 * rgb.g + 0.0722 * rgb.b;
			//brightness = bitmap_getpixel(bmap, x2, y2) & 0xff;
			samples[si] = brightness;
		}
		else
			samples[si] = 0;
	}
}

struct bitmap *bitmap_edge_sobel(const struct bitmap *bmap)
{
	uint32_t samples[6];

	struct bitmap *bmap_edges;

	int x, y;
	double eh, ev;
	double c;

	if (!(bmap_edges = bitmap_new(bmap->w, bmap->h))) {
		fputs("ERROR: (sobel) Could not alloc memory for edge image\n", stderr);
		return NULL;
	}

	for (x = 2; x < bmap->w - 2; x++) {
		for (y = 2; y < bmap->h - 2; y++) {
			struct rgb255 rgb;

			get_sobel_samples(bmap, x, y, samples, 1);
			eh = (samples[0] + 2 * samples[1] + samples[2])
					- (samples[3] + 2 * samples[4] + samples[5]);
			get_sobel_samples(bmap, x, y, samples, 0);
			ev = (samples[0] + 2 * samples[1] + samples[2])
					- (samples[3] + 2 * samples[4] + samples[5]);
			c = sqrt((eh * eh) + (ev * ev));

			while (c > 255)
				c = log(c); //c = sqrt(c);//c = pow(c, 0.3);// c /= 64;
			if (c < 32)
				c = 0;
			rgb.r = rgb.g = rgb.b = c;// & 0xff;
			bitmap_setpixel(bmap_edges, fromRGB(&rgb), x, y);
		}
	}
	return bmap_edges;

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