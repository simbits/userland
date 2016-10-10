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

// Video deocode demo using OpenMAX IL though the ilcient helper library

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "bcm_host.h"
#include "ilclient.h"

static int repeats = 0;
static int loopcounter = 0;

#define kRendererInputPort 90

static int video_decode_test(char *filename, int layer, off_t seek, int alpha)
{
   OMX_VIDEO_PARAM_PORTFORMATTYPE format;
   OMX_TIME_CONFIG_CLOCKSTATETYPE cstate;
   OMX_CONFIG_DISPLAYREGIONTYPE displayconfig;
   COMPONENT_T *video_decode = NULL, *video_scheduler = NULL, *video_render = NULL, *clock = NULL;
   COMPONENT_T *list[5];
   TUNNEL_T tunnel[4];
   ILCLIENT_T *client;
   FILE *in;
   int status = 0;
   int loopcount = 0;
   unsigned int data_len = 0;
   struct stat st;
   off_t filesize = 0L;

   memset(list, 0, sizeof(list));
   memset(tunnel, 0, sizeof(tunnel));

   if (stat(filename, &st)) {
       printf("Could not stat file: '%s'\n", filename);
       return -2;
   }

   filesize = st.st_size;
   printf("Opening '%s'(%lld bytes)\n", filename, filesize);

   if((in = fopen(filename, "rb")) == NULL)
      return -2;


   if((client = ilclient_init()) == NULL)
   {
      fclose(in);
      return -3;
   }

   if(OMX_Init() != OMX_ErrorNone)
   {
      ilclient_destroy(client);
      fclose(in);
      return -4;
   }

   // create video_decode
   if(ilclient_create_component(client, &video_decode, "video_decode", ILCLIENT_DISABLE_ALL_PORTS | ILCLIENT_ENABLE_INPUT_BUFFERS) != 0)
      status = -14;
   list[0] = video_decode;

   // create video_render
   if(status == 0 && ilclient_create_component(client, &video_render, "video_render", ILCLIENT_DISABLE_ALL_PORTS) != 0)
      status = -14;
   list[1] = video_render;

   // create clock
   if(status == 0 && ilclient_create_component(client, &clock, "clock", ILCLIENT_DISABLE_ALL_PORTS) != 0)
      status = -14;
   list[2] = clock;

   memset(&displayconfig, 0, sizeof(displayconfig));
   displayconfig.nSize = sizeof(displayconfig);
   displayconfig.nVersion.nVersion = OMX_VERSION;
   displayconfig.set = (OMX_DISPLAYSETTYPE)(OMX_DISPLAY_SET_LAYER | OMX_DISPLAY_SET_ALPHA);
   displayconfig.nPortIndex = kRendererInputPort;
   displayconfig.layer = layer;
   displayconfig.alpha = alpha;
   printf ("layer: %d, alpha %d\n", (int)displayconfig.layer, (int)displayconfig.alpha);
   if (video_render != NULL && OMX_SetParameter(ILC_GET_HANDLE(video_render), OMX_IndexConfigDisplayRegion, &displayconfig) != OMX_ErrorNone) {
      status = -13;
      printf ("OMX_IndexConfigDisplayRegion failed\n");
   }

   memset(&cstate, 0, sizeof(cstate));
   cstate.nSize = sizeof(cstate);
   cstate.nVersion.nVersion = OMX_VERSION;
   cstate.eState = OMX_TIME_ClockStateWaitingForStartTime;
   cstate.nWaitMask = 1;
   if(clock != NULL && OMX_SetParameter(ILC_GET_HANDLE(clock), OMX_IndexConfigTimeClockState, &cstate) != OMX_ErrorNone)
      status = -13;

   // create video_scheduler
   if(status == 0 && ilclient_create_component(client, &video_scheduler, "video_scheduler", ILCLIENT_DISABLE_ALL_PORTS) != 0)
      status = -14;
   list[3] = video_scheduler;

   set_tunnel(tunnel, video_decode, 131, video_scheduler, 10);
   set_tunnel(tunnel+1, video_scheduler, 11, video_render, 90);
   set_tunnel(tunnel+2, clock, 80, video_scheduler, 12);

   // setup clock tunnel first
   if(status == 0 && ilclient_setup_tunnel(tunnel+2, 0, 0) != 0)
      status = -15;
   else
      ilclient_change_component_state(clock, OMX_StateExecuting);

   if(status == 0)
      ilclient_change_component_state(video_decode, OMX_StateIdle);


   memset(&format, 0, sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE));
   format.nSize = sizeof(OMX_VIDEO_PARAM_PORTFORMATTYPE);
   format.nVersion.nVersion = OMX_VERSION;
   format.nPortIndex = 130;
   format.eCompressionFormat = OMX_VIDEO_CodingAVC;

   if(status == 0 &&
      OMX_SetParameter(ILC_GET_HANDLE(video_decode), OMX_IndexParamVideoPortFormat, &format) == OMX_ErrorNone &&
      ilclient_enable_port_buffers(video_decode, 130, NULL, NULL, NULL) == 0)
   {
      OMX_BUFFERHEADERTYPE *buf;
      int port_settings_changed = 0;
      int first_packet = 1;
      int rotatecounter = 0;
      char cntr = 0x00;

      ilclient_change_component_state(video_decode, OMX_StateExecuting);

      if (seek != 0 && seek < filesize) {
         printf("Seeking to: %lld from %lld\n", seek, filesize);
         fseek(in, seek, SEEK_SET);
      }

      while((buf = ilclient_get_input_buffer(video_decode, 130, 1)) != NULL)
      {
         // feed data and wait until we get port settings changed
         unsigned char *dest = buf->pBuffer;

         data_len += fread(dest, 1, buf->nAllocLen-data_len, in);

         if(port_settings_changed == 0 &&
            ((data_len > 0 && ilclient_remove_event(video_decode, OMX_EventPortSettingsChanged, 131, 0, 0, 1) == 0) ||
             (data_len == 0 && ilclient_wait_for_event(video_decode, OMX_EventPortSettingsChanged, 131, 0, 0, 1,
                                                       ILCLIENT_EVENT_ERROR | ILCLIENT_PARAMETER_CHANGED, 10000) == 0)))
         {
            printf("1: port settings changed\n"); fflush(stdout);
            port_settings_changed = 1;

            printf("2: setup_tunnel\n"); fflush(stdout);
            if(ilclient_setup_tunnel(tunnel, 0, 0) != 0)
            {
               status = -7;
               break;
            }

            printf("3: change_component_state\n"); fflush(stdout);
            ilclient_change_component_state(video_scheduler, OMX_StateExecuting);

            // now setup tunnel to video_render
            printf("4: setup_tunnel\n");
            if(ilclient_setup_tunnel(tunnel+1, 0, 1000) != 0)
            {
               status = -12;
               break;
            }

            printf("5: change_component_state\n"); fflush(stdout);
            ilclient_change_component_state(video_render, OMX_StateExecuting);
         }

         if (!data_len) {
            printf("loop: %d of %d\n", ++loopcounter, repeats);
            if (repeats && loopcounter >= repeats) break;
            fseek(in, 0L, SEEK_SET);
         }

         buf->nFilledLen = data_len;
         data_len = 0;

         buf->nOffset = 0;
         printf(".");

         if(first_packet)
         {
            printf("first packet\n"); fflush(stdout);
            buf->nFlags = OMX_BUFFERFLAG_STARTTIME;
            first_packet = 0;
         }
         else
            buf->nFlags = OMX_BUFFERFLAG_TIME_UNKNOWN;

          if(OMX_EmptyThisBuffer(ILC_GET_HANDLE(video_decode), buf) != OMX_ErrorNone)
         {
            status = -6;
            break;
         }
/*
         switch (rotatecounter++) {
             case 0: printf("\r /            ");  break;
             case 1:
             case 3: printf("\r -            ");  break;
             case 2: printf("\r \\            "); break;
         }
         fflush(stdout);
         rotatecounter %= 3;
*/
      }

      printf("BREAK with status: %d\n", status); fflush(stdout);
      buf->nFilledLen = 0;
      buf->nFlags = OMX_BUFFERFLAG_TIME_UNKNOWN | OMX_BUFFERFLAG_EOS;

      printf("EmptyThisBuffer\n"); fflush(stdout);
      if(OMX_EmptyThisBuffer(ILC_GET_HANDLE(video_decode), buf) != OMX_ErrorNone)
         status = -20;

      // wait for EOS from render
      printf("wait_for_event (EOS)\n"); fflush(stdout);
      ilclient_wait_for_event(video_render, OMX_EventBufferFlag, 90, 0, OMX_BUFFERFLAG_EOS, 0,
                              ILCLIENT_BUFFER_FLAG_EOS, 10000);

      // need to flush the renderer to allow video_decode to disable its input port
      printf("wait_flush_tunnels\n"); fflush(stdout);
      ilclient_flush_tunnels(tunnel, 0);


      printf("disable_port_buffers\n"); fflush(stdout);
      ilclient_disable_port_buffers(video_decode, 130, NULL, NULL, NULL);
   }

   printf("fclose\n");
   fclose(in);

   ilclient_disable_tunnel(tunnel);
   ilclient_disable_tunnel(tunnel+1);
   ilclient_disable_tunnel(tunnel+2);
   ilclient_teardown_tunnels(tunnel);

   ilclient_state_transition(list, OMX_StateIdle);
   ilclient_state_transition(list, OMX_StateLoaded);

   ilclient_cleanup_components(list);

   OMX_Deinit();

   ilclient_destroy(client);
   return status;
}

int main (int argc, char **argv)
{
    char *path = NULL;
    int layer = 0;
    int alpha = 0;
    off_t seek = 0L;

   if (argc < 6) {
      printf("Usage: %s <repeats> <layer> <alpha> <seek> <filename>\n", argv[0]);
      exit(1);
   }

   repeats = atoi(argv[1]);
   layer = atoi(argv[2]);
   alpha = atoi(argv[3]);
   seek = atoll(argv[4]);
   path = argv[5];

   bcm_host_init();
   video_decode_test(path, layer, seek, alpha);
}
