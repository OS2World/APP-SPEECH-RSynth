#include <config.h>
#include "proto.h"
#include <io.h>
#include <stdio.h>
#include "phones.h"

void *dict;
char **dialect = ph_am;

#ifdef HAVE_LIBGDBM
#include <useconfig.h>
#include <ctype.h>
#include <gdbm.h>

#include "dict.h"
#include "getargs.h"

#ifndef DICT_DIR
#define DICT_DIR "."
#endif

char *dict_path = "a";


unsigned char *
dict_find(s, n)
char *s;
unsigned n;
{
 if (!n)
  n = strlen(s);
 if (dict)
  {
   datum key;
   datum data;
   int i;
   key.dptr = malloc(n);
   key.dsize = n;
   for (i = 0; i < n; i++)
    {
     key.dptr[i] = (islower(s[i])) ? toupper(s[i]) : s[i];
    }
   data = gdbm_fetch((GDBM_FILE) dict, key);
   free(key.dptr);
   if (data.dptr)
    {
     unsigned char *w = (unsigned char *) malloc(data.dsize + 1);
     memcpy(w, data.dptr, data.dsize);
     w[data.dsize] = 0;
     free(data.dptr);
     return w;
    }
  }
 return NULL;
}

static void choose_dialect PROTO((void));

static void
choose_dialect()
{
 unsigned char *word = dict_find("schedule", 0);
 if (word)
  {
   if (word[0] == SH)
    dialect = ph_br;
   else if (word[0] == S && word[1] == K)
    dialect = ph_am;
   free(word);
  }
}

int
dict_init(argc, argv)
int argc;
char *argv[];
{
 char *buf = NULL;
 char *dictdir;
 argc = getargs("Dictionary", argc, argv, "d", "", &dict_path, "Which dictionary [b|a]", NULL);
 dictdir = getenv("ETC");
 if (!help_only)
  {
   buf = malloc(strlen(dictdir) + strlen("Dict.db") + strlen(dict_path) + 2);
   sprintf(buf, "%sDict.db", dict_path);
   if (!(dict = gdbm_open(buf, GDBM_READER, 0, 0666, NULL)))
    {                
     sprintf(buf, "%s/%sDict.db", dictdir, dict_path);
     dict = gdbm_open(buf, GDBM_READER, 0, 0666, NULL);
    }                

  if (dict)         
    {                
     dict_path = realloc(buf, strlen(buf) + 1);
    }                
   if (dict)         
    choose_dialect();
  }
 return argc;
}

void
dict_term()
{
 if (dict)
  gdbm_close((GDBM_FILE) dict);
}

#else

unsigned char *
dict_find(s, n)
char *s;
unsigned n;
{
 return NULL;
}

int
dict_init(argc, argv)
int argc;
char *argv[];
{
 return argc;
}

void
dict_term()
{

}

#endif
