/* stralloc.h --- Bibliothèque d'allocation de chaînes de caractères.  */

#include <stdlib.h>

/* `String' et le type des chaînes de caractères.  */
typedef struct String String;
typedef struct Char Char;

/* Allocation d'une chaîne de `size` bytes.  */
String *str_alloc (size_t size);

/* Taille en bytes de la chaîne `str`.  */
size_t str_size (String *str);

/* Pointeur sur le tableau de bytes de la chaîne `str`.  */
char *str_data (String *str);

/* Libère l'espace occupé par la chaîne `str`.  */
void str_free (String *str);

/* Renvoie la concaténation des deux chaînes `s1` et `s2`.  */
String *str_concat (String *s1, String *s2);

/* Compacte l'espace occupé par toutes les chaînes de caractères, de manière
   à éliminer la framgmentation.  Vous pouvez présumer que le client
   ne va pas utiliser `str_data' pendant la compaction ni utiliser après
   la compaction un str_data obtenu auparavant.  */
void str_compact (void);

/* Renvoie la somme des `str_size` des chaînes actuellement utilisées.  */
size_t str_livesize (void);

/* Renvoie le nombre de bytes disponibles dans la "free list".  */
size_t str_freesize (void);

/* Renvoie le nombre de bytes alloués par la librairie (via mmap).  */
size_t str_usedsize (void);
