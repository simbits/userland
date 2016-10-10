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

#include "bcm_host.h"

#ifndef ALIGN_UP
#define ALIGN_UP(x,y)  ((x + (y)-1) & ~((y)-1))
#endif

#ifndef ALIGN_TO_16
#define ALIGN_TO_16(x)  ((x + 15) & ~15)
#endif

#define ELEMENT_CHANGE_LAYER          (1<<0)
#define ELEMENT_CHANGE_OPACITY        (1<<1)
#define ELEMENT_CHANGE_DEST_RECT      (1<<2)
#define ELEMENT_CHANGE_SRC_RECT       (1<<3)
#define ELEMENT_CHANGE_MASK_RESOURCE  (1<<4)
#define ELEMENT_CHANGE_TRANSFORM      (1<<5)

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
                           uint32_t rgba)
{
    int         row;
    int         col;

    uint32_t *line = (uint32_t *)image + (y * (pitch>>2)) + x;

    for ( row = 0; row < h; row++ )
    {
        for ( col = 0; col < w; col++ )
        {
            line[col] = rgba;
        }
        line += (pitch>>2);
    }
}

int main(void)
{
    RECT_VARS_T    *vars;
    uint32_t        screen = 0;
    int             ret;
    VC_RECT_T       src_rect;
    VC_RECT_T       dst_rect;
    VC_IMAGE_TYPE_T type = VC_IMAGE_RGBA32;
    int width;
    int height;
    int pitch;
    int aligned_height;
    uint8_t ctr = 0;

    VC_DISPMANX_ALPHA_T alpha = {
        DISPMANX_FLAGS_ALPHA_FROM_SOURCE,
        0, /*alpha 0->255*/
        0
    };

    vars = &gRectVars;

    bcm_host_init();

    printf("Open display[%i]...\n", screen );
    vars->display = vc_dispmanx_display_open( screen );

    ret = vc_dispmanx_display_get_info( vars->display, &vars->info);
    assert(ret == 0);
    printf( "Display is %d x %d\n", vars->info.width, vars->info.height );

    width = vars->info.width;
    height = vars->info.height;
    pitch = (ALIGN_TO_16(width) * 32) / 8;
    aligned_height = ALIGN_TO_16(height);

    vars->image = calloc( 1, pitch * height );
    assert(vars->image);

    FillRectRGB32( type, vars->image, pitch, aligned_height,   0,  0, width, height, 0x00000000 );

    for (ctr=0; ctr<8; ctr++) {
        FillRectRGB32( type, vars->image, pitch, aligned_height, 96 + (ctr * 64),  20, 64, 40,  0xff000000 | (1 << (16 + ctr)));
        FillRectRGB32( type, vars->image, pitch, aligned_height, 96 + (ctr * 64),  60, 64, 40,  0xff000000 | (1 << (8 + ctr)));
        FillRectRGB32( type, vars->image, pitch, aligned_height, 96 + (ctr * 64), 100, 64, 40,  0xff000000 | (1 << ctr));
        FillRectRGB32( type, vars->image, pitch, aligned_height, 96 + (ctr * 64), 140, 64, 40,  0xff000000 | (1 << ctr) | (1 << (8 + ctr)) | (1 << (16 + ctr)));
    }

    for (ctr=0; ctr<255; ctr++) {
        FillRectRGB32( type, vars->image, pitch, aligned_height, 32 + ctr*2, 180, 2, 30,  0xff000000 | (ctr << 16) );
        FillRectRGB32( type, vars->image, pitch, aligned_height, 32 + ctr*2, 220, 2, 30,  0xff000000 | (ctr << 8) );
        FillRectRGB32( type, vars->image, pitch, aligned_height, 32 + ctr*2, 260, 2, 30,  0xff000000 | (ctr << 0) );
        FillRectRGB32( type, vars->image, pitch, aligned_height, 32 + ctr*2, 300, 2, 30,  0xff000000 | (ctr << 16) | (ctr << 8) | ctr );
    }

    for (ctr=0; ctr<255; ctr++) {
        FillRectRGB32( type, vars->image, pitch, aligned_height, 353 + ctr, 180, 1 , 150 , 0x00ffffff | (ctr << 24));
    }

    vars->resource = vc_dispmanx_resource_create( type,
                                                  width,
                                                  height,
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

    vc_dispmanx_rect_set( &src_rect, 0, 0, width << 16, height << 16);

    vc_dispmanx_rect_set( &dst_rect, 0,
                                     0,
                                     width,
                                     height );

    vars->element = vc_dispmanx_element_add( vars->update,
                                             vars->display,
                                             2000,               // layer
                                             &dst_rect,
                                             vars->resource,
                                             &src_rect,
                                             DISPMANX_PROTECTION_NONE,
                                             &alpha,
                                             NULL,             // clamp
                                             VC_IMAGE_ROT0 );

    ret = vc_dispmanx_update_submit_sync( vars->update );
    assert( ret == 0 );

    for (ctr=0; ctr<10; ctr++) {
       printf("\r%2d", ctr);
       fflush(stdout);
       sleep(1);
    }
    printf("Bye\n");

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

