#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <png.h>

#include <ft2build.h>
#include FT_FREETYPE_H

typedef struct pixel_t {
    uint8_t r;
    uint8_t g;
    uint8_t b;
    uint8_t a;
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

    int pixel_size = 4;
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
                  PNG_COLOR_TYPE_RGBA,
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
            *row++ = p->a;
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
#define COLOUR_BLACK   0,   0,   0

void colour_set (Image *i, uint32_t x, uint32_t y, uint8_t r, uint8_t g, uint8_t b)
{
    Pixel *p = pixel_get (i, x, y);
    p->r = r;
    p->g = g;
    p->b = b;
    p->a = 255;
}

void transparent_set (Image *i, uint32_t x, uint32_t y)
{
    Pixel *p = pixel_get (i, x, y);
    p->r = 0;
    p->g = 0;
    p->b = 0;
    p->a = 0;
}

#define CARD_WIDTH  40
#define CARD_HEIGHT 64
#define TEXT_TOP 5
#define TEXT_LEFT 5

static char letters[] = {'A', '2', '3', '4', '5', '6', '7', '8', '9', '0', 'J', 'Q', 'K'};

int main (int argc, char**argv)
{
    Image image;

    /* Initialize FreeType2 */
    FT_Library ft_library;
    FT_Face    ft_face;

    if (FT_Init_FreeType (&ft_library))
    {
        fprintf (stderr, "Error: Unable to initialize FreeType2.\n");
        return EXIT_FAILURE;
    }

    /* Load the font */
    if (FT_New_Face (ft_library, "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf", 0, &ft_face))
    {
        fprintf (stderr, "Error: Unable to load font.\n");
        return EXIT_FAILURE;
    }

    /* Set the font size */
    if (FT_Set_Char_Size (ft_face, 0, 7*64,  /* 7 pt */
                                  96, 96    /* 96 dpi */))
    {
        fprintf (stderr, "Error: Unable to set font size.\n");
        return EXIT_FAILURE;
    }

    /* Create an image */
    image.width = 1024;
    image.height = 256;
    image.data = calloc (image.width * image.height, sizeof (Pixel));

    if (!image.data)
    {
        fprintf (stderr, "Error: Unable to allocate memory for pixel data.\n");
        return EXIT_FAILURE;
    }

    /* Initialize to white */
    for (uint32_t y = 0; y < image.height; y++)
    {
        for (uint32_t x = 0; x < image.width; x++)
        {
            transparent_set (&image, x, y);
        }
    }

    for (uint32_t card_col = 0; card_col < 16; card_col++)
    {
        for (uint32_t card_row= 0; card_row < 4; card_row++)
        {
            /* Top and bottom */
            for (uint32_t x = 1; x < CARD_WIDTH - 1; x++)
            {
                colour_set (&image, x + card_col * CARD_WIDTH,
                                    0 + card_row * CARD_HEIGHT, COLOUR_BLACK);
                colour_set (&image, x + card_col * CARD_WIDTH,
                                   63 + card_row * CARD_HEIGHT, COLOUR_BLACK);
            }
            /* Left and right */
            for (uint32_t y = 1; y < CARD_HEIGHT - 1; y++)
            {
                colour_set (&image, 0 + card_col * CARD_WIDTH,
                                    y + card_row * CARD_HEIGHT, COLOUR_BLACK);
                colour_set (&image,39 + card_col * CARD_WIDTH,
                                    y + card_row * CARD_HEIGHT, COLOUR_BLACK);
            }
            /* White cards */
            for (uint32_t x = 1; x < CARD_WIDTH - 1; x++)
            {
                for (uint32_t y = 1; y < CARD_HEIGHT - 1; y++)
                {
                    colour_set (&image, x + card_col * CARD_WIDTH,
                                        y + card_row * CARD_HEIGHT, COLOUR_WHITE);
                }
            }
            /* A letter or number in the top corner */
            /* glyph->bitmap_left,
             * glyph->bitmap_top (distance from baseline to top of character,
             * glyph->advance (increase in pen position, 'escapement')
             * Docs reccomend treating the bitmap as an alpha channel and blending with gamma correction */
            if (card_col < sizeof(letters))
            {
                if (FT_Load_Char (ft_face, (uint32_t) letters[card_col], FT_LOAD_RENDER))
                {
                    fprintf (stderr, "Error: Unable to set load glyph.\n");
                    return EXIT_FAILURE;
                }

                for (uint32_t x = 0; x < ft_face->glyph->bitmap.width; x++)
                {
                    for (uint32_t y = 0; y < ft_face->glyph->bitmap.rows; y++)
                    {
                        uint32_t pixel_index = x + y * ft_face->glyph->bitmap.pitch;
                        /* Top left */
                        colour_set (&image, card_col * CARD_WIDTH + x + TEXT_LEFT,
                                            card_row * CARD_HEIGHT + y + TEXT_TOP,
                                            card_row < 2 ?
                                                255 : 255 - (uint8_t)ft_face->glyph->bitmap.buffer[pixel_index],
                                            255 - (uint8_t)ft_face->glyph->bitmap.buffer[pixel_index],
                                            255 - (uint8_t)ft_face->glyph->bitmap.buffer[pixel_index]);
                        /* Bottom right */
                        colour_set (&image, card_col * CARD_WIDTH + (CARD_WIDTH - (x + TEXT_LEFT)),
                                            card_row * CARD_HEIGHT + (CARD_HEIGHT - (y + TEXT_TOP)),
                                            card_row < 2 ?
                                            255 : 255 - (uint8_t)ft_face->glyph->bitmap.buffer[pixel_index],
                                            255 - (uint8_t)ft_face->glyph->bitmap.buffer[pixel_index],
                                            255 - (uint8_t)ft_face->glyph->bitmap.buffer[pixel_index]);
                    }
                }
            }

        }
    }

    /* Card borders */

    export (&image, "cards.png");

    free (image.data);

    return EXIT_SUCCESS;
}
