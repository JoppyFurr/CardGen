#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <png.h>

typedef struct pixel_t {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} Pixel;

typedef struct image_t {
    Pixel *data;
    uint32_t width;
    uint32_t height;
} Image;

static Pixel *pixel_get (Image *i, uint32_t x, uint32_t y)
{
    return &i->data[i->width * y + x];
}

static int export (Image *i, const char *path)
{
    FILE *file = fopen (path, "wb");

    png_structp png_ptr = NULL;
    png_infop info_ptr = NULL;
    uint32_t x, y;
    png_byte ** row_pointers = NULL;

    int pixel_size = 3;
    int depth = 8;

    if (!file)
    {
        fprintf (stderr, "Error: Unable to open file %s for writing.\n", path);
        return EXIT_FAILURE;
    }

    png_ptr = png_create_write_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png_ptr)
    {
        fprintf (stderr, "Error: png_create_write_struct returns NULL.\n");
        return EXIT_FAILURE;
    }

    info_ptr = png_create_info_struct (png_ptr);

    if (!info_ptr)
    {
        fprintf (stderr, "Error: png_create_info_struct returns NULL.\n");
        return EXIT_FAILURE;
    }

    /* Set image attributes */
    png_set_IHDR (png_ptr, info_ptr, i->width, i->height, depth,
                  PNG_COLOR_TYPE_RGB,
                  PNG_INTERLACE_NONE,
                  PNG_COMPRESSION_TYPE_DEFAULT,
                  PNG_FILTER_TYPE_DEFAULT);

    /* Initialize rows of PNG */
    row_pointers = png_malloc (png_ptr, i->height * sizeof (png_byte *));

    for (uint32_t y = 0; y < i->height; y++)
    {
        png_byte *row = png_malloc (png_ptr, i->width * pixel_size);
        if (!row)
        {
            fprintf (stderr, "There was a trouble.\n");
        }
        row_pointers[y] = row;

        for (uint32_t x = 0; x < i->width; x++)
        {
            Pixel *p = pixel_get(i, x, y);
            *row++ = p->r;
            *row++ = p->g;
            *row++ = p->b;
        }
    }

    /* Write to file */
    png_init_io (png_ptr, file);
    png_set_rows (png_ptr, info_ptr, row_pointers);
    png_write_png (png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

    /* Tidy up */
    for (uint32_t y = 0; y  < i->height; y++)
    {
        png_free (png_ptr, row_pointers[y]);
    }
    png_free (png_ptr, row_pointers);

    return EXIT_SUCCESS;
}

#define COLOUR_WHITE 255, 255, 255
void colour_set (Image *i, uint32_t x, uint32_t y, uint8_t r, uint8_t g, uint8_t b)
{
    Pixel *p = pixel_get (i, x, y);
    p->r = r;
    p->g = g;
    p->b = b;
}

int main (int argc, char**argv)
{
    Image image;
    uint32_t x;
    uint32_t y;

    /* Create an image */
    image.width = 1024;
    image.height = 256;
    image.data = calloc (image.width * image.height, sizeof (Pixel));

    if (!image.data)
    {
        return EXIT_FAILURE;
    }

    /* Initialize to white */
    for (y = 0; y < image.height; y++)
    {
        for (x = 0; x < image.width; x++)
        {
            colour_set (&image, x, y, COLOUR_WHITE);
        }
    }

    export (&image, "cards.png");

    free (image.data);

    return EXIT_SUCCESS;
}
