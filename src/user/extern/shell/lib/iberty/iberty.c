/*
* @Author: mstg
* @Date:   2016-05-22 02:26:08
* @Last Modified by:   mstg
* @Last Modified time: 2016-05-22 02:28:36
*/
/*
  Most of this library is from gcc libiberty
*/

#include "iberty.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

void freeargv (char **vector) {
  register char **scan;

  if (vector != NULL)
  {
    for (scan = vector; *scan != NULL; scan++)
    {
      free (*scan);
    }
    free (vector);
  }
}

char **buildargv (const char *input, int *ac) {
  char *arg;
  char *copybuf;
  int squote = 0;
  int dquote = 0;
  int bsquote = 0;
  int argc = 0;
  int maxargc = 0;
  char **argv = NULL;
  char **nargv;

  if (input != NULL)
  {
    copybuf = (char *) malloc (strlen (input) + 1);
      /* Is a do{}while to always execute the loop once.  Always return an
      argv, even for null strings.  See NOTES above, test case below. */
    do
    {
      /* Pick off argv[argc] */
      while (isblank (*input))
      {
        input++;
      }
      if ((maxargc == 0) || (argc >= (maxargc - 1)))
      {
        /* argv needs initialization, or expansion */
        if (argv == NULL)
        {
          maxargc = INITIAL_MAXARGC;
          nargv = (char **) malloc (maxargc * sizeof (char *));
        }
        else
        {
          maxargc *= 2;
          nargv = (char **) realloc (argv, maxargc * sizeof (char *));
        }
        if (nargv == NULL)
        {
          if (argv != NULL)
          {
            freeargv (argv);
            argv = NULL;
          }
          break;
        }
        argv = nargv;
        argv[argc] = NULL;
      }
    /* Begin scanning arg */
      arg = copybuf;
      while (*input != EOS)
      {
        if (isspace (*input) && !squote && !dquote && !bsquote)
        {
          break;
        }
        else
        {
          if (bsquote)
          {
            bsquote = 0;
            *arg++ = *input;
          }
          else if (*input == '\\')
          {
            bsquote = 1;
          }
          else if (squote)
          {
            if (*input == '\'')
            {
              squote = 0;
            }
            else
            {
              *arg++ = *input;
            }
          }
          else if (dquote)
          {
            if (*input == '"')
            {
              dquote = 0;
            }
            else
            {
              *arg++ = *input;
            }
          }
          else
          {
            if (*input == '\'')
            {
              squote = 1;
            }
            else if (*input == '"')
            {
              dquote = 1;
            }
            else
            {
              *arg++ = *input;
            }
          }
          input++;
        }
      }
      *arg = EOS;
      argv[argc] = strdup (copybuf);
      if (argv[argc] == NULL)
      {
        freeargv (argv);
        argv = NULL;
        break;
      }
      argc++;
      argv[argc] = NULL;

      while (isspace (*input))
      {
        input++;
      }
    }
    while (*input != EOS);
  }

  *ac = argc;
  return (argv);
}
