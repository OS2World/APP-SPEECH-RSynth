#include <config.h>
#include "proto.h"
#include <stdio.h>
#include <fcntl.h>
#include <useconfig.h>
#include "getargs.h"
#include "l2u.h"
#include "hplay.h"
#include "file.h"

#define SUN_MAGIC 	0x2e736e64		/* Really '.snd' */
#define SUN_HDRSIZE	24			/* Size of minimal header */
#define SUN_UNSPEC	((unsigned)(~0))	/* Unspecified data size */
#define SUN_ULAW	1			/* u-law encoding */
#define SUN_LIN_8	2			/* Linear 8 bits */
#define SUN_LIN_16	3			/* Linear 16 bits */

file_write_p file_write = NULL;
file_term_p  file_term  = NULL;

static char *linear_file;
static char *au_file;
static char *wav_file;
static int au_fd = -1;            /* file descriptor for .au ulaw file */
static int linear_fd = -1;
static int wav_fd = -1;

static unsigned au_encoding = SUN_ULAW;
static unsigned au_size = 0;
static unsigned wav_size = 0;

static void wblong PROTO((int fd, unsigned long x));
static void 
wblong(fd, x)
int fd;
unsigned long x;
{
 int i;
 for (i = 24; i >= 0; i -= 8)
  {
   char byte = (char) ((x >> i) & 0xFF);
   write(fd, &byte, 1);
  }
}

static void walong PROTO((int fd, unsigned long x));
static void 
walong(fd, x)
int fd;
unsigned long x;
{
 int i;
 for (i = 0; i <= 24; i += 8)
  {
   unsigned char byte = (unsigned char) ((x >> i) & 0xFF);
   write(fd, &byte, 1);
  }
}


extern void au_header PROTO((int fd,unsigned enc,unsigned rate,unsigned size,char *comment));

void 
au_header(fd, enc, rate, size, comment)
int fd;
unsigned enc;
unsigned rate;
unsigned size;
char *comment;
{
 if (!comment)
  comment = "";
 wblong(fd, SUN_MAGIC);
 wblong(fd, SUN_HDRSIZE + strlen(comment));
 wblong(fd, size);
 wblong(fd, enc);
 wblong(fd, rate);
 wblong(fd, 1);                   /* channels */
 write(fd, comment, strlen(comment));
}

void WriteWaveHeader(int fh, int n)
{
    long bl;
    int  bi;

    write(fh, "RIFF",4);	// Write "RIFF"
    bl  = n;
    bl += 36L;
    walong(fh,bl); 	// Write Size of file with header
    write(fh, "WAVE",4);	// Write "WAVE"
    write(fh, "fmt ",4);	// Write "fmt "
    bl = 0x00000010;
    walong(fh,bl);	// Size of previous header (fixed)
    bl = 0x00010001;
    walong(fh, bl);	// formatTag and nChannels
    bl = samp_rate;     
    walong(fh, bl);	// nSamplesPerSec
    walong(fh, bl); 	// nAvgBytesPerSec
    bl = 0x00080001;
    walong(fh,bl);	  // nBlockAlign (always 1?) / nBitsPerSample
    write(fh,"data",4);     // Write "data"
    bl = n;
    walong(fh, bl);	// True length of sample data
}

static void aufile_write PROTO((int n,short *data));

static void
aufile_write(n, data)
int n;
short *data;
{
 if (n > 0)
  {
   if (linear_fd >= 0)
    {
     unsigned size = n * sizeof(short);
     if (write(linear_fd, data, n * sizeof(short)) != size)
            perror("write");
    }
   if (au_fd >= 0)
    {
     if (au_encoding == SUN_LIN_16)
      {
       unsigned size = n * sizeof(short);
       if (write(au_fd, data, size) != size)
        perror("write");
       else
        au_size += size;
      }
     else if (au_encoding == SUN_ULAW)
      {
       unsigned char *plabuf = (unsigned char *) malloc(n);
       if (plabuf)
        {
         unsigned char *p = plabuf;
         unsigned char *e = p + n;
         while (p < e)
          {
           *p++ = short2ulaw(*data++);
          }
         if (write(au_fd, plabuf, n) != n)
          perror(au_file);
         else
          au_size += n;
         free(plabuf);
        }
       else
        {
         fprintf(stderr, "%s : No memory for ulaw data\n", program);
        }
      }
     else
      {
       abort();
      }
    }
  }
 if (wav_fd >= 0)
  {
    unsigned char *plabuf = (unsigned char *) malloc(n);
    WriteWaveHeader(wav_fd,n);
    if (plabuf)
      {
        unsigned char *p = plabuf;
        unsigned char *e = p + n;
        while (p < e)
         {
          *data /= 128  ;
          *p = *data + 128;
           p++;
           data++;
          }
        if (write(wav_fd, plabuf, n) != n)
          perror(wav_file);
       else 
          wav_size += n;
       free(plabuf);
      }
     else
      {
       fprintf(stderr, "%s : No memory for wav data\n", program);
       return;
      }
  }
}

static void aufile_term PROTO((void));

static void
aufile_term()
{
 /* Finish ulaw file */
 if (au_fd >= 0)
  {
   off_t here = lseek(au_fd, 0L, SEEK_CUR);
   if (here >= 0)
    {
     /* can seek this file - truncate it */
     ftruncate(au_fd, here);
     /* Now go back and overwite header with actual size */
     if (lseek(au_fd, 8L, SEEK_SET) == 8)
      {
       wblong(au_fd, au_size);
      }
    }
   if (au_fd != 1)
    close(au_fd);
   au_fd = -1;
  }
 /* Finish linear file */
 if (linear_fd >= 0)
  {
   ftruncate(linear_fd, lseek(linear_fd, 0L, SEEK_CUR));
   if (linear_fd != 1)
    close(linear_fd);
   linear_fd = -1;
  }
/* finish WAV file */
 if (wav_fd >= 0)
  {
   off_t here = lseek(wav_fd, 0L, SEEK_CUR);
   if (here >= 0)
    {
     /* can seek this file - truncate it */
     ftruncate(wav_fd, here);
     /* Now go back and overwite header with actual size */
     if (lseek(wav_fd, 4L, SEEK_SET) == 4)
      {
       walong(wav_fd, wav_size+36);
      }
     if (lseek(wav_fd, 40L, SEEK_SET) == 40)
      {
       walong(wav_fd, wav_size);
      }

    }
   if (wav_fd != 1)
    close(wav_fd);
   wav_fd = -1;
  }

}


int
file_init(argc, argv)
int argc;
char *argv[];
{
 argc = getargs("File output", argc, argv,
                "l", "", &linear_file, "Raw 16-bit linear pathname",
                "o", "", &au_file,     "Sun/Next audio file name",
                "w", "", &wav_file,    "OS/2 WAV File name",
                NULL);
 if (help_only)
  return argc;

 if (au_file)
  {
   if (strcmp(au_file, "-") == 0)
    {
     au_fd = 1;                   /* stdout */
    }
   else
    {
     au_fd = open(au_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
     if (au_fd < 0)
      perror(au_file);
    }
   if (au_fd >= 0)
    {
     if (samp_rate > 8000)
      au_encoding = SUN_LIN_16;
     else
      au_encoding = SUN_ULAW;
     au_header(au_fd, au_encoding, samp_rate, SUN_UNSPEC, "");
     au_size = 0;
    }
  }

 if (linear_file)
  {
   if (strcmp(linear_file, "-") == 0)
    {
     linear_fd = 1 /* stdout */ ;
    }
   else
    {
     linear_fd = open(linear_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
     if (linear_fd < 0)
      perror(linear_file);
    }
  }
 if (wav_file)
  {
   if (strcmp(wav_file, "-") == 0)
    {
     wav_fd = 1 /* stdout */ ;
    }
   else
    {
     wav_fd = open(wav_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
     if (wav_fd < 0)
      perror(wav_file);
    }
  }


 if (au_fd >= 0 || linear_fd >= 0 || wav_fd > 0)
  {
   file_write = aufile_write;
   file_term  = aufile_term;
  }
 return argc;
}


