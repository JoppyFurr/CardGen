#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include <png.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#define CARD_WIDTH  40
#define CARD_HEIGHT 64
#define TEXT_TOP 4
#define TEXT_LEFT 4

#define TEXT_POINT 8
#define SUIT_POINT 9

typedef struct Colour_t {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} Colour;

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

static const Colour COLOUR_WHITE = {255, 255, 255};
static const Colour COLOUR_BLACK = {  0,   0,   0};
static const Colour COLOUR_RED   = {255,   0,   0};
static const Colour COLOUR_GREEN = {  0, 255,   0};
static const Colour COLOUR_SKY   = {128, 128, 255};
static const Colour COLOUR_CYAN  = {  0, 255, 255};

static Image image;
static FT_Library ft_library;
static FT_Face ft_face_text;
static FT_Face ft_face_symbol;
static char *card_values[] = {"A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K"};
static uint32_t card_suits[]  = {0x2665 /* ♥ */, 0x2666 /* ♦ */, 0x2663 /* ♣ */, 0x2660 /* ♠*/};


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

void colour_set (Image *i, uint32_t x, uint32_t y, Colour c)
{
    Pixel *p = pixel_get (i, x, y);
    p->r = c.r;
    p->g = c.g;
    p->b = c.b;
    p->a = 255;
}

/* Assumes the existing pixel has alpha of either 0 or 255 */
void draw_colour_over (Image *i, uint32_t x, uint32_t y, Colour c, uint8_t a)
{
    double a_float = a / 255.0;
    Pixel *p = pixel_get (i, x, y);
    if (p->a) /* Opaque target */
    {
        p->r = (1.0 - a_float) * p->r + a_float * c.r;
        p->g = (1.0 - a_float) * p->r + a_float * c.g;
        p->b = (1.0 - a_float) * p->r + a_float * c.b;
    }
    else /* Transparent target */
    {
        p->r = c.r;
        p->g = c.g;
        p->b = c.b;
        p->a = a;
    }
}

void transparent_set (Image *i, uint32_t x, uint32_t y)
{
    Pixel *p = pixel_get (i, x, y);
    p->r = 0;
    p->g = 0;
    p->b = 0;
    p->a = 0;
}

uint32_t draw_card_glyph (uint32_t card_col, uint32_t card_row, uint32_t x_offset, uint32_t y_offset,
                     FT_Face ft_face, uint32_t font_size, Colour colour, uint32_t c, bool mirror)
{
    /* Possibly useful fields:
     * glyph->bitmap_left,
     * glyph->bitmap_top (distance from baseline to top of character,
     * Docs reccomend treating the bitmap as an alpha channel and blending with gamma correction */
    /* Set the font size */
    if (FT_Set_Char_Size (ft_face, 0, font_size << 6,
                                  96, 96    /* 96 dpi */))
    {
        fprintf (stderr, "Error: Unable to set font size.\n");
        return EXIT_FAILURE;
    }

    if (FT_Load_Char (ft_face, c, FT_LOAD_RENDER))
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
            draw_colour_over (&image, card_col * CARD_WIDTH + x + x_offset,
                                      card_row * CARD_HEIGHT + y + y_offset,
                                      colour, ft_face->glyph->bitmap.buffer[pixel_index]);
            if (mirror)
            {
                /* Bottom right */
                draw_colour_over (&image, card_col * CARD_WIDTH  + (CARD_WIDTH  - (x + x_offset)),
                                          card_row * CARD_HEIGHT + (CARD_HEIGHT - (y + y_offset)),
                                          colour, ft_face->glyph->bitmap.buffer[pixel_index]);
            }
        }
    }

    return ft_face->glyph->advance.x >> 6; /* Advance is stored in 1/64th pixels */
}

uint32_t draw_card_glyph_centre (uint32_t card_col, uint32_t card_row, FT_Face ft_face, uint32_t font_size, Colour colour, uint32_t c)
{
    /* Possibly useful fields:
     * glyph->bitmap_left,
     * glyph->bitmap_top (distance from baseline to top of character,
     * Docs reccomend treating the bitmap as an alpha channel and blending with gamma correction */
    /* Set the font size */
    if (FT_Set_Char_Size (ft_face, 0, font_size << 6,
                                  96, 96    /* 96 dpi */))
    {
        fprintf (stderr, "Error: Unable to set font size.\n");
        return EXIT_FAILURE;
    }

    if (FT_Load_Char (ft_face, c, FT_LOAD_RENDER))
    {
        fprintf (stderr, "Error: Unable to set load glyph.\n");
        return EXIT_FAILURE;
    }

    uint32_t x_offset = (CARD_WIDTH - ft_face->glyph->bitmap.width) / 2;
    uint32_t y_offset = (CARD_HEIGHT - ft_face->glyph->bitmap.rows) / 2;

    /* TODO: Common blit function */
    for (uint32_t x = 0; x < ft_face->glyph->bitmap.width; x++)
    {
        for (uint32_t y = 0; y < ft_face->glyph->bitmap.rows; y++)
        {
            uint32_t pixel_index = x + y * ft_face->glyph->bitmap.pitch;
            draw_colour_over (&image, card_col * CARD_WIDTH + x + x_offset,
                                      card_row * CARD_HEIGHT + y + y_offset,
                                      colour, ft_face->glyph->bitmap.buffer[pixel_index]);
        }
    }

    return ft_face->glyph->advance.x >> 6; /* Advance is stored in 1/64th pixels */
}
void draw_card_background (uint32_t card_col, uint32_t card_row)
{
    for (uint32_t x = 1; x < CARD_WIDTH - 1; x++)
    {
        for (uint32_t y = 1; y < CARD_HEIGHT - 1; y++)
        {
            colour_set (&image, x + card_col * CARD_WIDTH,
                                y + card_row * CARD_HEIGHT, COLOUR_WHITE);
        }
    }
}

void draw_card_outline (uint32_t card_col, uint32_t card_row)
{
            /* Top and bottom */
            for (uint32_t x = 2; x < CARD_WIDTH - 2; x++)
            {
                colour_set (&image, x + card_col * CARD_WIDTH,
                                    0 + card_row * CARD_HEIGHT, COLOUR_BLACK);
                colour_set (&image, x + card_col * CARD_WIDTH,
                                   63 + card_row * CARD_HEIGHT, COLOUR_BLACK);
            }
            /* Left and right */
            for (uint32_t y = 2; y < CARD_HEIGHT - 2; y++)
            {
                colour_set (&image, 0 + card_col * CARD_WIDTH,
                                    y + card_row * CARD_HEIGHT, COLOUR_BLACK);
                colour_set (&image,39 + card_col * CARD_WIDTH,
                                    y + card_row * CARD_HEIGHT, COLOUR_BLACK);
            }
            /* Curved corner */
            colour_set (&image, 1 + card_col * CARD_WIDTH,
                                1 + card_row * CARD_HEIGHT, COLOUR_BLACK);

            colour_set (&image, 1 + card_col * CARD_WIDTH,
                                CARD_HEIGHT - 2 + card_row * CARD_HEIGHT, COLOUR_BLACK);
            colour_set (&image, CARD_WIDTH - 2 + card_col * CARD_WIDTH,
                                1 + card_row * CARD_HEIGHT, COLOUR_BLACK);
            colour_set (&image, CARD_WIDTH - 2 + card_col * CARD_WIDTH,
                                CARD_HEIGHT - 2 + card_row * CARD_HEIGHT, COLOUR_BLACK);
}

int main (int argc, char**argv)
{


    /* Initialize FreeType2 */
    if (FT_Init_FreeType (&ft_library))
    {
        fprintf (stderr, "Error: Unable to initialize FreeType2.\n");
        return EXIT_FAILURE;
    }

    /* Load the font */
    if (FT_New_Face (ft_library, "/usr/share/fonts/truetype/noto/NotoSans-Regular.ttf", 0, &ft_face_text))
    {
        fprintf (stderr, "Error: Unable to load text font.\n");
        return EXIT_FAILURE;
    }
    if (FT_New_Face (ft_library, "/usr/share/fonts/truetype/noto/NotoSansSymbols-Regular.ttf", 0, &ft_face_symbol))
    {
        fprintf (stderr, "Error: Unable to load symbol font.\n");
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

    /* Initialize to transparent */
    for (uint32_t y = 0; y < image.height; y++)
    {
        for (uint32_t x = 0; x < image.width; x++)
        {
            transparent_set (&image, x, y);
        }
    }

    /* A 13 × 4 block of playing cards */
    for (uint32_t card_col = 0; card_col < 13; card_col++)
    {
        for (uint32_t card_row= 0; card_row < 4; card_row++)
        {
            draw_card_background (card_col, card_row);

            draw_card_outline (card_col, card_row);

            /* A letter or number in the top corner */
            if (card_col < sizeof(card_values) / sizeof(card_values[0]))
            {
                uint32_t escapement = 0;

                for (char *c = card_values[card_col]; *c != '\0'; c++)
                {
                    escapement += draw_card_glyph (card_col, card_row, TEXT_LEFT + escapement, TEXT_TOP, /* Position */
                                                   ft_face_text, TEXT_POINT, card_row < 2 ? COLOUR_RED : COLOUR_BLACK, /* Font */
                                                   *c, true); /* Glyph + mirror */
                }
            }
            /* Card symbols - TODO: Get these to line up with the number, or beside the number */
            if (card_col < sizeof(card_values) / sizeof(card_values[0]))
            {
                 draw_card_glyph (card_col, card_row, TEXT_LEFT, TEXT_TOP + 10, /* Position */
                                  ft_face_text, SUIT_POINT, card_row < 2 ? COLOUR_RED : COLOUR_BLACK, /* font */
                                  card_suits[card_row], true); /* Glyph + mirror */
            }

        }
    }

    /* Special cards */
    /* 1: Blank - An outline that can be used as a place holder */
    {
        uint32_t card_col = 13;
        uint32_t card_row = 0;
        draw_card_outline (card_col, card_row);
    }
    /* 2: A recycle symbol for when the stock runs dry */
    {
        uint32_t card_col = 13;
        uint32_t card_row = 1;
        draw_card_outline (card_col, card_row);
        draw_card_glyph_centre (card_col, card_row, ft_face_symbol, 24, COLOUR_GREEN, 0x21b6 /* refresh symbol */);
    }
    /* 3: The back of a card */
    {
        uint32_t card_col = 13;
        uint32_t card_row = 2;

        draw_card_background (card_col, card_row);

        draw_card_outline (card_col, card_row);

        /* Blue rectangle pattern */
        for (uint32_t x = 4; x < CARD_WIDTH - 4; x++)
        {
            for (uint32_t y = 4; y < CARD_HEIGHT - 4; y++)
            {
                if ((x + y) & 1)
                {
                    colour_set (&image, x + card_col * CARD_WIDTH,
                                        y + card_row * CARD_HEIGHT, COLOUR_SKY);
                }
                else
                {
                    colour_set (&image, x + card_col * CARD_WIDTH,
                                        y + card_row * CARD_HEIGHT, COLOUR_CYAN);
                }
            }
        }
        /* Round the corners */
        colour_set (&image, 4 + card_col * CARD_WIDTH, 4 + card_row * CARD_HEIGHT, COLOUR_WHITE);
        colour_set (&image, CARD_WIDTH - 5 + card_col * CARD_WIDTH, 4 + card_row * CARD_HEIGHT, COLOUR_WHITE);
        colour_set (&image, 4 + card_col * CARD_WIDTH, CARD_HEIGHT - 5 + card_row * CARD_HEIGHT, COLOUR_WHITE);
        colour_set (&image, CARD_WIDTH - 5 + card_col * CARD_WIDTH, CARD_HEIGHT - 5 + card_row * CARD_HEIGHT, COLOUR_WHITE);
    }

    export (&image, "cards.png");

    free (image.data);

    return EXIT_SUCCESS;
}
