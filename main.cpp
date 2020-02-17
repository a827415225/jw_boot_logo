/* Simple tool to generate a bootloader image blob to replace
 * bmp.blob for the nVIDIA Jetson JetPack 3.2 distribution.
 * Put in the public domain in May 2018. Use only at your 
 * own risk, no guarantees of merchantability or fitness for 
 * any particular purpose (nor of safety or correctness) is 
 * made or implied.
 */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"


unsigned char *rescale_720(unsigned char const *i1080);
unsigned char *rescale_crop_480(unsigned char const *i1080);
void swapperize(unsigned char *b, long y, long x);

extern unsigned char header1[];
extern size_t size1;
extern unsigned char header2[];
extern size_t size2;
extern unsigned char header3[];
extern size_t size3;
extern unsigned char header4[];
extern size_t size4;

int main(int argc, char const *argv[]) {
    if ((argc != 2 && argc != 4) || (argv[1][0] == '-')) {
        fprintf(stderr, "usage: jw_boot_logo input1080p.{jpg/tga/png} [input720p.xxx input480p.xxx]\n");
        fprintf(stderr, "outputs a file named bmp.blob in the working directory.\n");
        fprintf(stderr, "Prepares a bmp.blob to use for Jetson bootloader to replace the start-up logo.\n");
        fprintf(stderr, "The input image should be 1920x1080 pixels; you can also provide 1280x720 and\n");
        fprintf(stderr, "640x480 versions, although these will be computed (scaled/cropped) from the\n");
        fprintf(stderr, "1080p if not provided. (You must provide 1 or 3 images.)\n");
        fprintf(stderr, "https://github.com/jwatte/jw_boot_logo\n");
        exit(1);
    }

    int w1080 = 0, h1080 = 0, tmp;
    unsigned char *i1080 = stbi_load(argv[1], &w1080, &h1080, &tmp, 3);
    if (!i1080) {
        fprintf(stderr, "%s: could not load image (TGA, JPG and PNG supported)\n", argv[1]);
        exit(1);
    }
    if (w1080 != 1920 || h1080 != 1080) {
        fprintf(stderr, "%s: must be 1920x1080; got %dx%d\n size", argv[1], w1080, h1080);
        exit(1);
    }

    unsigned char *i720;
    unsigned char *i480;
    int w, h;
    if (argc == 4) {
        i720 = stbi_load(argv[2], &w, &h, &tmp, 3);
        if (!i720) {
            fprintf(stderr, "%s: could not load image\n", argv[2]);
            exit(1);
        }
        if (w != 1280 || h != 720) {
            fprintf(stderr, "%s: must be 1280x720, got %dx%d size\n", argv[2], w, h);
            exit(1);
        }
        i480 = stbi_load(argv[3], &w, &h, &tmp, 3);
        if (!i480) {
            fprintf(stderr, "%s: could not load image\n", argv[3]);
            exit(1);
        }
        if (w != 640 || h != 480) {
            fprintf(stderr, "%s: must be 640x480, got %dx%d size\n", argv[3], w, h);
            exit(1);
        }
    } else {
        i720 = rescale_720(i1080);
        i480 = rescale_crop_480(i1080);
    }

    FILE *f = fopen("bmp.blob", "wb");
    if (!f) {
        fprintf(stderr, "Could not create bmp.blob (do you have write permission?)\n");
        exit(1);
    }
    if (size1 != fwrite(header1, 1, size1, f)) {
        perror("bmp.blob (header1)");
        exit(1);
    }
    if (size2 != fwrite(header2, 1, size2, f)) {
        perror("bmp.blob (header2)\n");
        exit(1);
    }
    swapperize(i480, 480, 640);
    if (640*480*3 != fwrite(i480, 1, 640*480*3, f)) {
        perror("bmp.blob (480 image)\n");
        exit(1);
    }
    if (size3 != fwrite(header3, 1, size3, f)) {
        perror("bmp.blob (header3)\n");
        exit(1);
    }
    swapperize(i720, 720, 1280);
    if (1280*720*3 != fwrite(i720, 1, 1280*720*3, f)) {
        perror("bmp.blob (720 image)\n");
        exit(1);
    }
    if (size4 != fwrite(header4, 1, size4, f)) {
        perror("bmp.blob (header4)\n");
        exit(1);
    }
    swapperize(i1080, 1080, 1920);
    if (1920*1080*3 != fwrite(i1080, 1, 1920*1080*3, f)) {
        perror("bmp.blob (1080 image)\n");
        exit(1);
    }
    fclose(f);
    free(i1080);
    free(i720);
    free(i480);
    fprintf(stderr, "created bmp.blob\n");
    return 0;
}

static void sample(unsigned char const *i, float y, float x, unsigned char *o) {
    unsigned int y0 = (unsigned int)y;
    unsigned int x0 = (unsigned int)x;
    unsigned int dy = y - y0;
    unsigned int dx = x - x0;
    unsigned int b = i[(y0*1920+x0)*3] * (1 - dy) * (1 - dx);
    unsigned int g = i[(y0*1920+x0)*3+1] * (1 - dy) * (1 - dx);
    unsigned int r = i[(y0*1920+x0)*3+2] * (1 - dy) * (1 - dx);
    b += i[(y0*1920+x0+1)*3] * (1 - dy) * dx;
    g += i[(y0*1920+x0+1)*3+1] * (1 - dy) * dx;
    r += i[(y0*1920+x0+1)*3+2] * (1 - dy) * dx;
    b += i[((y0+1)*1920+x0)*3] * dy * (1 - dx);
    g += i[((y0+1)*1920+x0)*3+1] * dy * (1 - dx);
    r += i[((y0+1)*1920+x0)*3+2] * dy * (1 - dx);
    b += i[((y0+1)*1920+x0+1)*3] * dy * dx;
    g += i[((y0+1)*1920+x0+1)*3+1] * dy * dx;
    r += i[((y0+1)*1920+x0+1)*3+2] * dy * dx;
    o[0] = (unsigned char)b;
    o[1] = (unsigned char)g;
    o[2] = (unsigned char)r;
}

unsigned char *rescale_720(unsigned char const *i1080) {
    unsigned char *ret = (unsigned char*)malloc(1280*720*3);
    unsigned char *d = ret;
    for (float y = 0; y != 720; y += 1.0f) {
        for (float x = 0; x != 1280; x += 1.0f) {
            sample(i1080, y * 1080 / 720 + 0.25f, x * 1920 / 1280 + 0.25f, d);
            d += 3;
        }
    }
    return ret;
}

unsigned char *rescale_crop_480(unsigned char const *i1080) {
    unsigned char *ret = (unsigned char*)malloc(640*480*3);
    unsigned char *d = ret;
    for (float y = 0; y != 480; y += 1.0f) {
        for (float x = 0; x != 640; x += 1.0f) {
            sample(i1080, y * 1080 / 480 + 0.5f, x * 1280 / 640 + (1920-1280)/2 + 0.5f, d);
            d += 3;
        }
    }
    return ret;
}


void swapperize(unsigned char *dst, long y, long x) {
    for (long i = 0; i < y/2; ++i) {
        unsigned char *a = &dst[i*x*3];
        unsigned char *b = &dst[(y-i-1)*x*3];
        for (long q = 0; q < x; ++q) {
            unsigned char r = a[2];
            a[2] = b[0];
            unsigned char g = a[1];
            a[1] = b[1];
            unsigned char l = a[0];
            a[0] = b[2];
            b[0] = r;
            b[1] = g;
            b[2] = l;
            a += 3;
            b += 3;
        }
    }
}


unsigned char header1[192] = {
0x4e,0x56,0x49,0x44,0x49,0x41,0x5f,0x5f,0x42,0x4c,0x4f,0x42,0x5f,0x5f,0x56,0x32,
0x00,0x00,0x02,0x00,0x64,0x2d,0x97,0x00,0x24,0x00,0x00,0x00,0x03,0x00,0x00,0x00,
0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xc0,0x00,0x00,0x00,0x38,0x10,0x0e,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xf8,0x10,0x0e,0x00,
0x36,0x30,0x2a,0x00,0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x2e,0x41,0x38,0x00,0x36,0xec,0x5e,0x00,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
};
size_t size1 = sizeof(header1);

unsigned char header2[54] = {
0x42,0x4d,0x38,0x10,0x0e,0x00,0x00,0x00,0x00,0x00,0x36,0x00,0x00,0x00,0x28,0x00,
0x00,0x00,0x80,0x02,0x00,0x00,0xe0,0x01,0x00,0x00,0x01,0x00,0x18,0x00,0x00,0x00,
0x00,0x00,0x02,0x10,0x0e,0x00,0x12,0x0b,0x00,0x00,0x12,0x0b,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,
};
size_t size2 = sizeof(header2);

unsigned char header3[56] = {
   0x00,0x00,
0x42,0x4d,0x36,0x30,0x2a,0x00,0x00,0x00,0x00,0x00,0x36,0x00,0x00,0x00,0x28,0x00,
0x00,0x00,0x00,0x05,0x00,0x00,0xd0,0x02,0x00,0x00,0x01,0x00,0x18,0x00,0x00,0x00,
0x00,0x00,0x00,0x30,0x2a,0x00,0x13,0x0b,0x00,0x00,0x13,0x0b,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,
};
size_t size3 = sizeof(header3);

unsigned char header4[54] = {
0x42,0x4d,0x36,0xec,0x5e,0x00,0x00,0x00,0x00,0x00,0x36,0x00,0x00,0x00,0x28,0x00,
0x00,0x00,0x80,0x07,0x00,0x00,0x38,0x04,0x00,0x00,0x01,0x00,0x18,0x00,0x00,0x00,
0x00,0x00,0x00,0xec,0x5e,0x00,0x01,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x00,0x00,
0x00,0x00,0x00,0x00,0x00,0x00,
};
size_t size4 = sizeof(header4);
