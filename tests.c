/* tests.c --- Programme de tests pour stralloc.  */

#include "stralloc.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

static void writestr (String *s)
{
  fwrite (str_data (s), 1, str_size (s), stdout);
}

static String *mkstr (const char *s)
{
  size_t len = strlen (s);
  String *str = str_alloc (len);
  memcpy (str_data (str), s, len);
  return str;
}

#define ASSERT(exp) test (__LINE__, exp)
static void test (int line, bool res)
{
  if (!res)
    printf ("Erreur de test à la ligne %d\n", line);
}


int main (int argc, char **argv)
{
  String *s1 = mkstr ("hello ");
  String *s2 = mkstr ("world ");
  String *s3 = str_concat (s1, s2);
  for (int i = 0; i < 20; i++) {
    String *s4 = str_concat (s3, s3);
    str_free (s3);
    s3 = s4;
  }
  /* À ce stade, seuls `s1`, `s2`, et `s3` sont "live".  */
  ASSERT (str_livesize () == str_size (s1) + str_size (s2) + str_size (s3));

  size_t before = str_usedsize ();
  str_compact ();
  /* La boucle ci-dessus a généré assez de gros trous pour que
     `str_compact` aie de quoi rendre de la mémoire au système!  */
  ASSERT (before > str_usedsize ());
  
  writestr (s1); writestr (s2); printf ("\n");

  /* ¡¡¡ COMPLÉTER ICI !!!    Ajoutez vos tests ici.  */
  
  size_t live = str_livesize ();
  size_t free = str_freesize ();
  size_t used = str_usedsize ();
  ASSERT (used > live + free);
  size_t overhead = used - live - free;
  printf ("Live = %uld, free = %uld, used = %uld\n", live, free, used);
  printf ("Overhead = %uld, i.e. %.1f%%\n",
          overhead, 100 * (double) overhead / used);
  return 0;
}
