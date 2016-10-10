/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// A simple demo using dispmanx to display an overlay

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <assert.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "bcm_host.h"

#define WIDTH   400
#define HEIGHT  400

#undef USE_SELECT

#ifndef ALIGN_UP
#define ALIGN_UP(x,y)  ((x + (y)-1) & ~((y)-1))
#endif

#define ELEMENT_CHANGE_LAYER          (1<<0)
#define ELEMENT_CHANGE_OPACITY        (1<<1)
#define ELEMENT_CHANGE_DEST_RECT      (1<<2)
#define ELEMENT_CHANGE_SRC_RECT       (1<<3)
#define ELEMENT_CHANGE_MASK_RESOURCE  (1<<4)
#define ELEMENT_CHANGE_TRANSFORM      (1<<5)

#define FADE_FIFO          "/tmp/fade_in.fifo"
#define NO_FADE            0
#define FADE_A_TO_BLACK    1
#define FADE_A_TO_WHITE    2
#define FADE_BLACK_TO_A    3
#define FADE_WHITE_TO_A    4

typedef struct
{
    DISPMANX_DISPLAY_HANDLE_T   display;
    DISPMANX_MODEINFO_T         info;
    void                       *image;
    DISPMANX_UPDATE_HANDLE_T    update;
    DISPMANX_RESOURCE_HANDLE_T  resource;
    DISPMANX_ELEMENT_HANDLE_T   element;
    uint32_t                    vc_image_ptr;

} RECT_VARS_T;

static RECT_VARS_T  gRectVars;

static void FillRectRGB32( VC_IMAGE_TYPE_T type,
                           void *image,
                           int pitch,
                           int aligned_height,
                           int x, int y, int w, int h,
                           int rgb)
{
    int         row;
    int         col;

    uint16_t *line = (uint16_t *)image + (y * (pitch>>1)) + x;

    for ( row = 0; row < h; row++ )
    {
        for ( col = 0; col < w; col++ )
        {
            line[col] = rgb;
        }
        line += (pitch>>1);
    }
}

int main(int argc, char *argv[])
{
    RECT_VARS_T    *vars;
    uint32_t        screen = 0;
    int             ret;
    VC_RECT_T       src_rect;
    VC_RECT_T       dst_rect;
    VC_IMAGE_TYPE_T type = VC_IMAGE_RGB565;
    int width = WIDTH, height = HEIGHT;
    int pitch = ALIGN_UP(width*2, 32);
    int aligned_height = ALIGN_UP(height, 16);
    int direction = 0;
    int fifo = 0;
    fd_set rfds;
    struct timeval tv;
    int fade_type = NO_FADE;
    int fade_udelay = 33000;

    if (argc < 3) {
        printf("Usage: %s <type> <delay us>\n", argv[0]);
        return 1;
    }

    fade_type = atoi(argv[1]);
    fade_udelay = atoi(argv[2]);

    printf("fading: %d - %d\n", fade_type, fade_udelay);

    VC_DISPMANX_ALPHA_T alpha = {
        DISPMANX_FLAGS_ALPHA_FROM_SOURCE | DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS,
        0, /*alpha 0->255*/
        0
    };

    vars = &gRectVars;

    bcm_host_init();
/*
    fifo = open(FADE_FIFO, O_RDONLY);
    if (fifo <= 0) {
        mkfifo(FADE_FIFO, 0666);
        while ((fifo = open(FADE_FIFO, O_RDWR)) <= 0) {
            printf("waiting for fifo: %s", FADE_FIFO);
            sleep(1);
        }
    }
*/

//  printf("Open display[%i]...\n", screen );
    vars->display = vc_dispmanx_display_open( screen );

    ret = vc_dispmanx_display_get_info( vars->display, &vars->info);
    assert(ret == 0);
//    printf( "Display is %d x %d\n", vars->info.width, vars->info.height );

    width = vars->info.width;
    height = vars->info.height;
    pitch = ALIGN_UP(width*2, 32);
    aligned_height = ALIGN_UP(height, 16);

    vars->image = calloc( 1, pitch * height );
    assert(vars->image);
    memset(vars->image, 0xff, pitch*height);

    vars->resource = vc_dispmanx_resource_create( type, width, height,
                                                  &vars->vc_image_ptr );
    assert( vars->resource );
    vc_dispmanx_rect_set( &dst_rect, 0, 0, width, height);
    ret = vc_dispmanx_resource_write_data(  vars->resource,
                                            type,
                                            pitch,
                                            vars->image,
                                            &dst_rect );
    assert( ret == 0 );
    vars->update = vc_dispmanx_update_start( 10 );
    assert( vars->update );

    vc_dispmanx_rect_set( &src_rect, 0, 0, width << 16, height << 16 );
    vc_dispmanx_rect_set( &dst_rect, 0, 0, width, height );
    vars->element = vc_dispmanx_element_add( vars->update,
                                             vars->display,
                                             500,               // layer
                                             &dst_rect,
                                             vars->resource,
                                             &src_rect,
                                             DISPMANX_PROTECTION_NONE,
                                             &alpha,
                                             NULL,             // clamp
                                             VC_IMAGE_ROT0 );

    ret = vc_dispmanx_update_submit_sync( vars->update );
    assert( ret == 0 );


    {
        char bfr[64];
        int retval;

/*
#ifdef USE_SELECT
        FD_ZERO(&rfds);
        FD_SET(fifo, &rfds);

        tv.tv_sec = 5;
        tv.tv_usec = 0;

        retval = select(1, &rfds, NULL, NULL, &tv);

        if (retval == -1)
            perror("select()");
        else if (retval) {
            char bfr[64];
            if (read(fifo, bfr, 1) > 0) {
                fade_type = bfr[1];
                printf("fade type: %d\n", fade_type);
            }
        }
#else
        if (read(fifo, bfr, 1) > 0) {
            fade_type = atoi(bfr);
            printf("fade type: %d\n", fade_type);
        } else {
            perror("fifo read: ");
            sleep(1);
        }
#endif
*/

        if (fade_type == FADE_A_TO_BLACK) {
            uint8_t a;
            int  gpio;
            printf("fade alpha to black\n");
            for (a = 0; a < 0xff; a++) {
                vars->update = vc_dispmanx_update_start( 0 );
                ret = vc_dispmanx_element_change_attributes( vars->update, vars->element,
                                                             ELEMENT_CHANGE_OPACITY,
                                                             0, a, NULL, NULL, 0, 0 );
                vc_dispmanx_update_submit_sync( vars->update );
                usleep(fade_udelay);
            }
            gpio = open("/sys/class/gpio/gpio19/value", O_WRONLY);
            write(gpio, "0", 1);
            close(gpio);
        } else if (fade_type == FADE_A_TO_WHITE) {
            printf("fade to white not yet implemented\n");
        } else if (fade_type == FADE_BLACK_TO_A) {
            uint8_t a;
            int  gpio;
            printf("fade black alpha\n");

            vars->update = vc_dispmanx_update_start( 0 );
            ret = vc_dispmanx_element_change_attributes( vars->update, vars->element,
                                                         ELEMENT_CHANGE_OPACITY,
                                                         0, 0xff, NULL, NULL, 0, 0 );
            ret = vc_dispmanx_update_submit_sync( vars->update );
            usleep(fade_udelay);

            gpio = open("/sys/class/gpio/gpio19/value", O_WRONLY);
            write(gpio, "1", 1);
            close(gpio);

            for (a = 0xfe; a > 0; a--) {
                vars->update = vc_dispmanx_update_start( 0 );
                ret = vc_dispmanx_element_change_attributes( vars->update, vars->element,
                                                             ELEMENT_CHANGE_OPACITY,
                                                             0, a, NULL, NULL, 0, 0 );
                ret = vc_dispmanx_update_submit_sync( vars->update );
                usleep(fade_udelay);
            }
        } else if (fade_type == FADE_WHITE_TO_A) {
            printf("fade to white not yet implemented\n");
        } else {
            printf("unknow type\n");
        }
    }

    vars->update = vc_dispmanx_update_start( 10 );
    assert( vars->update );
    ret = vc_dispmanx_element_remove( vars->update, vars->element );
    assert( ret == 0 );
    ret = vc_dispmanx_update_submit_sync( vars->update );
    assert( ret == 0 );
    ret = vc_dispmanx_resource_delete( vars->resource );
    assert( ret == 0 );
    ret = vc_dispmanx_display_close( vars->display );
    assert( ret == 0 );

    return 0;
}
