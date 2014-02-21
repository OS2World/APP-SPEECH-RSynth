#include <config.h>
#include <stdio.h>
#include <float.h>
#include <os2.h>
#define  INCL_OS2MM
#include <os2me.h>
#include "proto.h"
#include "getargs.h"
#include "hplay.h"

/* PlayList Entry */

typedef struct _PLE {
    ULONG operation;
    ULONG operand1;
    ULONG operand2;
    ULONG operand3;
}PLE;

long samp_rate  = 8000;
long bits = 8;
int quiet = FALSE;
int chipmunk = FALSE;
MCI_OPEN_PARMS mop;
PLE playlist[2];

void mci_err(ULONG rc)
{
    const rsize = 128;
    char rbuff[rsize];

    ULONG rc2 = mciGetErrorString(rc,      // error code
                                  rbuff,   // return buffer
                                  rsize);  // rbuff size

    if (rc2 == MCIERR_SUCCESS)
        fprintf(stderr,"MCI error: %s\n\n",rbuff);
    else
        fprintf(stderr,"error # %d has occured!\n\n", rc);
}

int
audio_init(int argc, char **argv)
{
 int rate;

 rate = 8;
 argc = getargs("Audio Initialization", argc, argv,
                "r", "%d",  &rate,   "Sample Rate in kHz - 8, 11, 22, or 44",
                "Q", NULL, &quiet,   "+Q for no sound output (-Q Default)",
	        "C", NULL, &chipmunk, "+C if speech sounds like chipmunks",
                "b", "%d", &bits, "Chunk size of playback sample 8 (def) or 16", 
                NULL);
 if (help_only)
  return (argc);
 
 switch(rate)
    {
     case  8 : samp_rate =  8000;
               break;
     case 11 : samp_rate = 11025;
               break;
     case 22 : samp_rate = 22050;
               break;
     case 44 : samp_rate = 44100;
               break;
     default : samp_rate =  8000;
    }

  if (bits != 16)
     bits = 8;

  return (argc);
}

void
audio_term(void)
{
}

void
audio_play(int n, short *data)
{
 MCI_PLAY_PARMS mpp;
 MCI_GENERIC_PARMS mgp;
 MCI_WAVE_SET_PARMS wsp;
 ULONG rc;

 if (!quiet)
  {
   unsigned char *plabuf;

   if (bits == 8)
     {
       plabuf = (unsigned char *) malloc(n);
       if (plabuf)
         {
          unsigned char *p = plabuf;
          unsigned char *e = p + n;
          short temp;
          while (p < e)
            {
             temp = *data / 128;
             *p = temp + 128;
             p++;
             data++;
            }
         }
       else 
         {
           fprintf(stderr, "Insufficient memory for Play Buffer\n\n");
           return;
         }
     }
   else
      plabuf = (char *)data;
   
   playlist[0].operation = DATA_OPERATION;
   playlist[0].operand1  = (long) plabuf;
   playlist[0].operand2  = n;

   if(bits == 16) playlist[0].operand2 = n*2;

   playlist[0].operand3  = 0;
   playlist[1].operation = EXIT_OPERATION;
   mop.hwndCallback   = 0;
   mop.usDeviceID     = 0;
   mop.pszDeviceType  = MCI_DEVTYPE_WAVEFORM_AUDIO_NAME;
   mop.pszElementName = (void *)&playlist[0];

   rc = mciSendCommand(0,
                       MCI_OPEN,                        // open message
                       MCI_WAIT | MCI_OPEN_SHAREABLE |  // message flags
                       MCI_OPEN_PLAYLIST,
                       &mop,                            // parameters
                       0);

   if (rc != MCIERR_SUCCESS) mci_err(rc);

   // set device parameters
   wsp.hwndCallback    = 0;
   if(chipmunk)
      wsp.ulSamplesPerSec = samp_rate/2;
   else
      wsp.ulSamplesPerSec = samp_rate;
   
   wsp.usBitsPerSample = bits;
   rc = mciSendCommand(mop.usDeviceID,
                       MCI_SET,
                       MCI_WAIT |
                       MCI_WAVE_SET_SAMPLESPERSEC |
                       MCI_WAVE_SET_BITSPERSAMPLE,
                       &wsp,
                       0);

    if (rc != MCIERR_SUCCESS) mci_err(rc);
    mpp.hwndCallback = 0;
    rc = mciSendCommand(mop.usDeviceID,
                        MCI_PLAY,
                        MCI_WAIT,
                        &mpp,
                        0);
     if (rc != MCIERR_SUCCESS) mci_err(rc);
     
     // close device

     mgp.hwndCallback = 0;
     rc = mciSendCommand(mop.usDeviceID,
                        MCI_CLOSE,
                        MCI_WAIT,
                        &mgp,
                        0);

     if (rc != MCIERR_SUCCESS) mci_err(rc);
     if (bits == 8)
        free(plabuf);
     _control87(EM_INVALID | EM_DENORMAL | EM_ZERODIVIDE | EM_OVERFLOW | EM_UNDERFLOW | EM_INEXACT, MCW_EM);

   }
}
