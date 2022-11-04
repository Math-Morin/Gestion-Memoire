/* stralloc.c --- Bibliothèque d'allocation de chaînes de caractères.  */

#include "stralloc.h"
#include <stdbool.h>
/* #include <stddef.h> */
#include <stdio.h>
/* #include <stdlib.h> */
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

typedef struct DataBlock DataBlock;
typedef struct StrBlock StrBlock;

struct DataBlock {
  int num; // debug
  size_t blocksize;
  size_t tailsize;
  size_t dataArraylen;
  size_t tailDataIndex;
  DataBlock* nextdatablock;
  char data[];
};

struct String {
  bool isfree;
  size_t len;
  size_t dataslot_size;
  String* nextfree;
  String* prevfree;
  char* strdata;
};

struct StrBlock {
  int num; // debug
  size_t blocksize;
  size_t strArraylen;
  size_t tailStringIndex;
  String* smallestfreeStr;
  StrBlock* nextstrblock;
  String str[];
};


static StrBlock* strblock_head = NULL;
static DataBlock* datablock_head = NULL;
static String* freelist_head = NULL;

static size_t livesize = 0;
static size_t totaldatasize = 0;


size_t max(size_t a, size_t b) {
  if (a >= b) return a;
  else        return b;
}

// Prend la longueur de la mémoire nécessaire pour entrposer la chaîne.
// Retourne un pointeur sur le premier String libre avec capacité
// suffisante pour prendre la chaine de caractères à mémoriser.
// Retourne NULL si aucune String dans la freelist.
String* getSmallestFreeStr(size_t size_needed) {
  printf("Entering getSmallestFreeStr().\n");
  printf("Size needed = %zu \n", size_needed);
  String* str_ptr = NULL;

  // Cherche un free String assez grand parmi tous les StrBlocks
  String* freelist_node = freelist_head;

  if (freelist_node) printf("freelist_head dataslot size: %zu\n",freelist_node->dataslot_size);
  else printf("freelist_head: null\n");

  while (freelist_node && freelist_node->dataslot_size < size_needed) {
    freelist_node = freelist_node->nextfree;
    if (freelist_node) printf("Next smallest dataslot size: %zu\n", freelist_node->dataslot_size);
    else printf("End of freelist reached.\n");
  }

  // Trouvé
  if (freelist_node) {
    printf("Free String found.\n");
    str_ptr = freelist_node;

    // remove string from freelist
    printf("Removing from freelist...\n");
    if (freelist_node->prevfree)
      freelist_node->prevfree->nextfree = freelist_node->nextfree;
    else
      freelist_head = freelist_node->nextfree;

    if (freelist_node->nextfree)
      freelist_node->nextfree->prevfree = freelist_node->prevfree;

    freelist_node->nextfree = NULL;
    freelist_node->prevfree = NULL;
    printf("String removed from freelist.\n");
  }

  if (str_ptr)
    printf("Returning str_ptr from getSmallestFreeStr().\n");
  else
    printf("Returning null str_ptr from getSmallestFreeStr().\n");
  return str_ptr;
}

// Prend la longueur de la mémoire nécessaire pour entrposer la chaîne.
// Retourne un pointeur sur le premier string de la 'tail',
// c-à-d les Strings qui n'ont encore jamais été aloués.
// Retourne NULL si aucune tail disponible.
String* getStrTail(size_t size_needed) {
  printf("Entering getStrTail().\n");
  printf("Size needed = %zu \n", size_needed);

  String* str_ptr = NULL;
  StrBlock* strblock = strblock_head;

  if (strblock) printf("Looking at strblock_head num: %d\n", strblock->num);
  else printf("strblock_head: null\n");

  // Cherche parmi les StrBlock qui ont une tail.
  // Un StrBlock dont la tail == strArraylen n'a plus de tail.
  while (strblock && !(strblock->tailStringIndex < strblock->strArraylen)) {
    printf("Looking ar strblock num: %d\n", strblock->num);
    printf("Tail index: %zu  max index: %zu\n", strblock->tailStringIndex, strblock->strArraylen-1);
    strblock = strblock->nextstrblock;
  }

  // Trouvé
  // TODO verifier et refactor getTail()
  if (strblock) {
    printf("String tail found in Strblock num: %d  at i:%zu\n", strblock->num, strblock->tailStringIndex);

    // Assigne le String à l'indice i au ptr à retourner
    size_t i = strblock->tailStringIndex;
    str_ptr = &strblock->str[i];

    // Ce String pointe maintenant vers un emplacement mémoire de capacité 'strlen' + 1
    str_ptr->dataslot_size = size_needed;
    printf("String dataslot size assigned to: %zu\n", str_ptr->dataslot_size);

    // Incrémente le pointeur de la 'tail'
    strblock->tailStringIndex += 1;
    printf("Tail index now: %zu\n", strblock->tailStringIndex);
  }

  if (str_ptr) printf("Returning str_ptr from getStrTail().\n");
  else printf("Returning null str_ptr from getStrTail().\n");
  return str_ptr;
}

// Ajoute un nouveau StrBlock.
// Prend la longueur de la mémoire nécessaire pour entrposer la chaîne.
// Retourne un pointeur sur le premier String de ce nouveau block.
String* addStrBlock(size_t size_needed) {
  printf("Entering addStrBlock().\n");
  printf("Size needed = %zu \n", size_needed);

  const size_t PAGESIZE = sysconf(_SC_PAGESIZE);
  printf("PAGESIZE = %zu.\n", PAGESIZE);
  const size_t ALLOC_SIZE = 5 * PAGESIZE; // TODO
  printf("ALLOC_SIZE = %zu.\n", ALLOC_SIZE);
  StrBlock* newstrblock = mmap (NULL, ALLOC_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0 );
  printf("New strblock allocated.\n");

  // Init le nouveau StrBlock
  newstrblock->blocksize = ALLOC_SIZE;
  printf("New strblock size = %zu\n", newstrblock->blocksize);
  newstrblock->strArraylen = (ALLOC_SIZE - sizeof(StrBlock)) / sizeof(String);
  printf("New strblock String array len = %zu\n", newstrblock->strArraylen);
  newstrblock->tailStringIndex = 1;
  printf("New strblock tail index = %zu\n", newstrblock->tailStringIndex);
  newstrblock->smallestfreeStr = NULL;
  newstrblock->nextstrblock = NULL;

  printf("Find new strblock position.\n");
  // Trouve ou placer le nouveau StrBlock.
  int lastblocknum = -1;
  if (strblock_head) {
    StrBlock* laststrblock = strblock_head;
    printf("Looking at strblock num: %d\n", laststrblock->num);
    lastblocknum = laststrblock->num;
    while (laststrblock && laststrblock->nextstrblock) {
      laststrblock = laststrblock->nextstrblock;
      printf("Looking at strblock num: = %d\n", laststrblock->num);
    }
    printf("No next found after strblock num:  %d\n", laststrblock->num);
    laststrblock->nextstrblock = newstrblock;
  }
  else {
    printf("No strblock head. Creating it.\n");
    strblock_head = newstrblock;
    printf("Head now of size: %zu\n", strblock_head->blocksize);
  }

  newstrblock->num = lastblocknum + 1;
  printf("New strblock given num: %d\n", newstrblock->num);

  // Init chacun des Strings du nouveau StrBlock
  // aux valeurs par défaut voulues.
  printf("Init new strblock Strings.\n");
  size_t lim = newstrblock->strArraylen;
  String* str;
  for (int i = 0; i < lim; i++) {
    str = &newstrblock->str[i];

    str->isfree = true;
    str->len = 0;
    str->dataslot_size = 0;
    str->nextfree = NULL;
    str->prevfree = NULL;
    str->strdata = NULL;
  }
  printf("Init done.\n");

  String* str_ptr = &newstrblock->str[0];
  str_ptr->dataslot_size = size_needed;
  printf("Assigned chosen String dataslot size to: %zu\n", str_ptr->dataslot_size);

  if (str_ptr) printf("Returning str_ptr from addStrBlock().\n");
  else printf("Returning null str_ptr from addStrBlock. ERROR!\n");
  return str_ptr; //TODO verifier et call getTail();
}


// Prend la longueur de la mémoire nécessaire pour entrposer la chaîne.
// Retourne un pointeur sur le String qui la contiendra.
// Ordre de priorité des Strings à retourner:
//   1. Strings déjà alloué mais devenu free (freelist)
//   2. String jamais alloué (tail)
//   3. Nouveau Strings d'un nouveau StrBlock fraichement ajouté.
String* getStrPtr(size_t size_needed) {
  printf("Entering getStrPtr().\n");
  String* str_ptr = NULL;

  str_ptr = getSmallestFreeStr(size_needed);

  if (!str_ptr) {
    printf("str_ptr is NULL. Trying getStrTail().\n");
    str_ptr = getStrTail(size_needed);
  }

  if (!str_ptr) {
    printf("str_ptr is NULL. Adding new strblock.\n");
    str_ptr = addStrBlock(size_needed);
  }

  printf("Returning str_ptr from getStrPtr().\n");
  return str_ptr;
}

// Prend la longueur de la mémoire nécessaire pour entrposer la chaîne.
// Retourne la première 'tail' rencontrée capable de contenir cette
// chaîne de caractères.
// Retourne NULL si aucun emplacement trouvé.
char* getDataTail(size_t size_needed) {
  printf("Entering getDataTail().\n");
  printf("Size needed = %zu \n", size_needed);

  char* data_ptr = NULL;
  DataBlock* datablock = datablock_head;

  if (datablock) printf("datablock_head num: %d\n", datablock->num);
  else printf("Datablock_head: null\n");

  // Cherche emplacement
  while (datablock && datablock->tailsize < size_needed) {
    printf("Not enough space. Datablock tail size: %zu  needed: %zu\n", datablock->tailsize, size_needed);
    printf("Going to next datablock. \n");
    datablock = datablock->nextdatablock;
  }

  if(datablock_head && !datablock) printf("There is no next datablock.\n");

  // Trouvé
  if (datablock) {
    printf("Tail found! Datablock tailsize: %zu  needed: %zu\n", datablock->tailsize, size_needed);
    // Assigne le pointeur à retourner vers l'emplacement de la 'tail'
    printf("Datablock tail found at index: %zu\n",datablock->tailDataIndex);
    size_t i = datablock->tailDataIndex;
    data_ptr = &datablock->data[i];

    // Déplace la 'tail' du datablock de strlen+1
    datablock->tailDataIndex += size_needed;
    printf("Datablock tail updated to index: %zu\n",datablock->tailDataIndex);
    // Diminue la mémoire libre du datablock de strlen+1
    datablock->tailsize -= size_needed;
    printf("Datablock tailsize updated to: %zu\n",datablock->tailsize);
    printf("Datablock array len - tail: %zu\n",datablock->dataArraylen - datablock->tailsize);
  }

  if (data_ptr) printf("Returning data_ptr from getDataTail().\n");
  else printf("Returning null data_ptr from getDataTail().\n");
  return data_ptr;
}

// Calcule la taille totale de la mémoire utilisée par les datablock.
size_t getDataMemUsed() {
  size_t mem_used = 0;

  DataBlock* data_block = datablock_head;
  while (data_block) {
    mem_used += data_block->blocksize;
    data_block = data_block->nextdatablock;
  }

  return mem_used;
}

// Prend la longueur de la mémoire nécessaire pour entrposer la chaîne.
// Retourne un pointeur sur la première case du tableau de charactères.
// La taille du nouveau datablock sera le double de la mémoire allouée jusqu'à ce moment.
// Si ceci est insuffisant, la taille du block sera ajustée à la hausse.
char* addDataBlock(size_t size_needed) {
  printf("Entering addDataBlock().\n");
  printf("Size needed = %zu \n", size_needed);

  const size_t PAGESIZE = sysconf(_SC_PAGESIZE);
  printf("PAGESIZE: %zu\n", PAGESIZE);
  const size_t DATA_MEM_USED = getDataMemUsed();
  printf("DATA_MEM_USED: %zu\n", DATA_MEM_USED);
  const size_t MIN_MEM_FOR_NEW_DATA =
    ((size_needed + sizeof(DataBlock)) / PAGESIZE + 1) * PAGESIZE;
  printf("MIN_MEM_FOR_NEW_DATA: %zu\n", MIN_MEM_FOR_NEW_DATA);
  const size_t ALLOC_SIZE = max(DATA_MEM_USED, MIN_MEM_FOR_NEW_DATA);
  printf("ALLOC_SIZE: %zu\n", ALLOC_SIZE);
  DataBlock* newdatablock = mmap (NULL, ALLOC_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0 );
  printf("New datablock allocated.\n");

  // Init du nouveau datablock
  newdatablock->blocksize = ALLOC_SIZE;
  printf("New datablock size: %zu\n", newdatablock->blocksize);
  newdatablock->tailsize = ALLOC_SIZE - sizeof(DataBlock) - size_needed;
  printf("New datablock tailsize: %zu\n", newdatablock->tailsize);
  newdatablock->dataArraylen = ALLOC_SIZE - sizeof(DataBlock);
  printf("New datablock dataArraylen: %zu\n", newdatablock->dataArraylen);
  newdatablock->tailDataIndex = size_needed;
  printf("New datablock tail index: %zu\n", newdatablock->tailDataIndex);
  newdatablock->nextdatablock = NULL;

  // Cherche la fin de la liste pour y ajouter le nouveau datablock
  printf("Searching for end of datablock list...\n");
  DataBlock* lastdatablock = datablock_head;
  if (lastdatablock) {
    printf("Looking at datablock num: %d\n", lastdatablock->num);
    while (lastdatablock && lastdatablock->nextdatablock) {
      lastdatablock = lastdatablock->nextdatablock;
      printf("Looking at datablock num: %d\n", lastdatablock->num);
    }
    printf("End found!. Block num %d has no next.\n", lastdatablock->num);
    lastdatablock->nextdatablock = newdatablock;
  }
  else {
    printf("Datablock list has no head. Creating it.\n");
    datablock_head = newdatablock; // Si aucun datablock
    datablock_head->num = 0;
    printf("Datablock head now of size: %zu\n", datablock_head->blocksize);
  }

  if (lastdatablock) newdatablock->num = lastdatablock->num + 1;
  printf("New datablock given num : %d \n", newdatablock->num);

  if (&newdatablock->data[0]) printf("Returning data_ptr from addDataBlock().\n");
  else printf("Returning null data_ptr from addDataBlock(). ERROR!\n");
  return &newdatablock->data[0];
}

// Prend la longueur de la mémoire nécessaire pour entrposer la chaîne.
// Retourne un pointeur sur l'emplacement approprié dans un datablock
// capable de contenir une chaîne de longueur 'strlen'.
char* getDataPtr(size_t size_needed) {
  printf("Entering getDataPtr().\n");
  char* data_ptr = NULL;

  data_ptr = getDataTail(size_needed);

  if (!data_ptr) {
    printf("data_ptr is null. Adding datablock.\n");
    data_ptr = addDataBlock(size_needed);
  }

  if (data_ptr) printf("Returning data_ptr from getDataPtr().\n");
  else printf("Returning null data_ptr from getDataPtr(). ERROR!\n");
  return data_ptr;
}

// Retourne un
String* str_alloc(size_t strlen) {
  printf("Entering str_alloc(). \n");
  size_t size_needed = strlen+1;

  String* str_ptr = getStrPtr(size_needed);
  printf("Back to str_alloc(). \n");

  // si ptr de freelist, pointe déjà vers son emplacement dans un datablock
  if (!str_ptr->strdata) {
    printf("String not pointing to any data location. Getting data location.\n");
    str_ptr->strdata = getDataPtr(size_needed);
  }

  printf("Updating string pointer...\n");
  str_ptr->isfree = false;
  printf("String isfree is now: %d\n", str_ptr->isfree);
  str_ptr->len = strlen; //TODO possiblement superflu.
  printf("String len is now: %zu\n", str_ptr->len);

  livesize += strlen; // ne compte pas les char null \0
  printf("Livesize is now: %zu\n", livesize);
  totaldatasize += size_needed;
  printf("Neededsize is now: %zu\n", totaldatasize);

  return str_ptr;
}

size_t str_size(String *str) {
  return str->len;
}

char* str_data(String *str) {
  if (str) return str->strdata;
  else return NULL;
}

// Libère l'espace occupé par la chaîne `str`.
void str_free(String *str) {
  printf("Entering str_free().\n");

  // Trouver position d'insertion dans la freelist
  printf("Searching insertion position into freelist for freed String of dataslot size: %zu\n", str->dataslot_size);
  String* freelist_node = freelist_head;
  if (freelist_node) {
    printf("First node of freelist is of size: %zu\n", freelist_node->dataslot_size);
    String* prev_node = NULL;

    while (freelist_node && freelist_node->dataslot_size < str->dataslot_size) {
      printf("Looking for next freelist node.\n");
      prev_node = freelist_node;
      printf("Previous freelist node was of dataslot size: %zu.\n", prev_node->dataslot_size);
      freelist_node = freelist_node->nextfree;
      if(freelist_node)
        printf("Candidate freelist node is of dataslot size: %zu. Looking for: > %zu\n", freelist_node->dataslot_size, str->dataslot_size);
    }

    if (freelist_node) printf("Insertion position found! Inserting before freelist node of dataslot size: %zu.\n", freelist_node->dataslot_size);
    else printf("Reached end of freelist. Inserting freed String at the end.\n");

    if (prev_node)
      prev_node->nextfree = str;
    else
      freelist_head = str;

    str->prevfree = prev_node;
    str->nextfree = freelist_node;

    if (freelist_node)
      freelist_node->prevfree = str;

    if (str->prevfree) printf("Prev free String is of dataslot size: %zu\n", str->prevfree->dataslot_size);
    else printf("Prev freelist node is NULL.\n");
    if (str->nextfree) printf("Next free String is of dataslot size: %zu\n", str->nextfree->dataslot_size);
    else printf("Next freelist node is NULL.\n");
  }
  else {
    printf("freelist_head : NULL. Adding String to freelist...\n");
    freelist_head = str;
    printf("freelist_head now pointing to String of dataslot size: %zu\n", str->dataslot_size);
  }

  str->isfree = true; // TODO superflu
  printf("isfree is now: %d\n",str->isfree);
  livesize -= str->len;
  printf("Livesize is now: %zu\n",livesize);
  totaldatasize -= str->len+1;
  printf("Neededsize is now: %zu\n",totaldatasize);
}

/* Renvoie la concaténation des deux chaînes `s1` et `s2`. */
String *str_concat(String* s1, String* s2) {
  size_t s1size = str_size (s1);
  printf("s1size : %zu\n", s1size);
  size_t s2size = str_size (s2);
  printf("s2size : %zu\n", s2size);
  String* s = str_alloc (s1size + s2size);
  char* sdata = str_data (s);
  memcpy (sdata, str_data (s1), s1size);
  strcpy (sdata + s1size, str_data (s2));
  return s;
}
/* Compacte l'espace occupé par toutes les chaînes de caractères, de manière
   à éliminer la framgmentation.  Vous pouvez présumer que le client
   ne va pas utiliser `str_data' pendant la compaction ni utiliser après
   la compaction un str_data obtenu auparavant.
   NOTE: demande une allocation memoire de meme taille que celle existante
   NOTE: parcours les pages FROM sequentiellement
   NOTE: copie les Strings rencontrees vers le TO
   NOTE: ajuste les pointeurs des Strings au fur et a mesure
   NOTE: ajuste les pointeurs du allocatedMem et freeList
   NOTE: munmap le FROM */
void str_compact () {
  printf("Entering str_compact().\n");

  const size_t PAGESIZE = sysconf(_SC_PAGESIZE);
  printf("PAGESIZE: %zu\n", PAGESIZE);
  printf("TOTAL_DATA_SIZE = %zu \n", totaldatasize);
  const size_t ALLOC_SIZE =
    ((totaldatasize + sizeof(DataBlock)) / PAGESIZE + 1) * PAGESIZE;
  printf("ALLOC_SIZE: %zu\n", ALLOC_SIZE);
  DataBlock* newdatablock = mmap (NULL, ALLOC_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0 );
  printf("New datablock allocated.\n");

  // Init du nouveau datablock
  newdatablock->num = 0;
  printf("New datablock given num: %d\n", newdatablock->num);
  newdatablock->blocksize = ALLOC_SIZE;
  printf("New datablock size: %zu\n", newdatablock->blocksize);
  newdatablock->tailsize = ALLOC_SIZE - sizeof(DataBlock);
  printf("New datablock tailsize: %zu\n", newdatablock->tailsize);
  newdatablock->dataArraylen = ALLOC_SIZE - sizeof(DataBlock);
  printf("New datablock dataArraylen: %zu\n", newdatablock->dataArraylen);
  newdatablock->tailDataIndex = 0;
  printf("New datablock tail index: %zu\n", newdatablock->tailDataIndex);
  newdatablock->nextdatablock = NULL;

  // Copie du live data vers nouveau datablock
  StrBlock* strblock = strblock_head;
  String* current_string;
  char* insert_pos;
  size_t tailIndex;
  size_t datalen;
  size_t lim;
  while (strblock) {
    printf("Starting copy of strings from strblock num: %d\n", strblock->num);
    lim = strblock->tailStringIndex;
    printf("Tail index at: %zu\n", strblock->tailStringIndex);
    for (int i = 0; i < lim; i++) {
      current_string = &strblock->str[i];
      printf("Looking at String at index: %d\n", i);
      if (!current_string->isfree) {
        printf("String is used. Start copy...\n");
        // copy data
        tailIndex = newdatablock->tailDataIndex;
        printf("Copying data to new datablock index: %zu\n", tailIndex);
        insert_pos = &newdatablock->data[tailIndex];
        strcpy(insert_pos, current_string->strdata);
        printf("Data copied to new datablock.\n");

        // update string pointer
        printf("Updating string pointer...\n");
        printf("Old data: %s\n", current_string->strdata);
        current_string->strdata = insert_pos;
        printf("New data: %s\n", insert_pos);
        datalen = current_string->len+1;
        printf("Updating String dataslot size from: %zu  to: %zu\n",
               current_string->dataslot_size, datalen);
        current_string->dataslot_size = datalen;

        // update newdatablock tail info
        printf("Updating new datablock tail info...\n");
        newdatablock->tailDataIndex += datalen;
        newdatablock->tailsize -= datalen;
        printf("Copy complete!\n");
      } else{
        printf("String is free. Skip.\n");
      }
    }
    strblock = strblock->nextstrblock;
    if (strblock) printf("Next strblock...\n");
    else printf("No more strblock.\n");
  }

  // unmap old datablocks
  printf("Unmap old datablocks.\n");
  DataBlock* current_datablock = datablock_head;
  DataBlock* next_datablock;
  while (current_datablock) {
    printf("Looking at datablock num: %d\n", current_datablock->num);
    next_datablock = current_datablock->nextdatablock;
    munmap(current_datablock, current_datablock->blocksize);
    printf("Datablock cleared.\n");
    current_datablock = next_datablock;
    if (current_datablock) printf("Next datablock...\n");
    else printf("No more datablock.\n");
  }

  // update freelist
  printf("Updating freelist...\n");
  String* freelist_node = freelist_head;
  while (freelist_node) {
    freelist_node->dataslot_size = -1; // HACK -1 == MAX SIZE_T sur machine complement à 2
    freelist_node->strdata = NULL;

    freelist_node = freelist_node->nextfree;
  }
  printf("Update of freelist complete.\n");

  datablock_head = newdatablock;
}

// Renvoie la somme des `str_size` des chaînes actuellement utilisées.
size_t str_livesize() {
  return livesize;
}

// Renvoie le nombre de bytes disponibles dans la "free list".
size_t str_freesize() {
  size_t freelist_freesize = 0;
  StrBlock* strblock = strblock_head;
  String* freelist_node;

  while (strblock) {
    freelist_node = strblock->smallestfreeStr;
    while (freelist_node) {
      freelist_freesize += freelist_node->dataslot_size;
      freelist_node = freelist_node->nextfree;
    }
    strblock = strblock->nextstrblock;
  }

  size_t tails_freesize = 0;
  DataBlock* datablock = datablock_head;

  while (datablock) {
    tails_freesize += datablock->tailsize;
    datablock = datablock->nextdatablock;
  }

  return freelist_freesize + tails_freesize;
}

// Renvoie le nombre de bytes alloués par la librairie (via mmap).
size_t str_usedsize() {
  size_t strblock_used = 0;
  StrBlock* strblock = strblock_head;
  while (strblock) {
    strblock_used += strblock->blocksize;
    strblock = strblock->nextstrblock;
  }

  size_t datablock_used = 0;
  DataBlock* datablock = datablock_head;
  while (datablock) {
    datablock_used += datablock->blocksize;
    datablock = datablock->nextdatablock;
  }

  return strblock_used + datablock_used;
}

static String* mkstr(const char *s)
{
  size_t len = strlen (s);
  printf("Making string for: %s  of size: %zu\n", s, len);

  String *str = str_alloc (len);
  strcpy(str_data (str), s);
  return str;
}

int main(int argc, char *argv[]) {
  printf("\n*** DEBUT MKSTR S1 ***\n");
  String *s1 = mkstr("1234");
  printf("\nS1: %s\n", str_data(s1));
  printf("*** FIN MKSTR S1 ***\n");

  printf("*****************\n");
  printf("usedsize: %zu\n", str_usedsize());
  printf("livesize: %zu\n", livesize);
  printf("freesize: %zu\n", str_freesize());
  printf("*****************\n");

  printf("\n*** DEBUT MKSTR S2 ***\n");
  String *s2 = mkstr("1234567");
  printf("\nS2: %s\n", str_data(s2));
  printf("*** FIN MKSTR S2 ***\n");

  printf("*****************\n");
  printf("usedsize: %zu\n", str_usedsize());
  printf("livesize: %zu\n", livesize);
  printf("freesize: %zu\n", str_freesize());
  printf("*****************\n");

  printf("\n*** DEBUT MKSTR S3 ***\n");
  String *s3 = mkstr("123456789");
  printf("\nS3: %s\n", str_data(s3));
  printf("*** FIN MKSTR S3 ***\n");

  printf("*****************\n");
  printf("usedsize: %zu\n", str_usedsize());
  printf("livesize: %zu\n", livesize);
  printf("freesize: %zu\n", str_freesize());
  printf("*****************\n");

  printf("\n*** DEBUT MKSTR S4 ***\n");
  String *s4 = mkstr("1");
  printf("\nS4: %s\n", str_data(s4));
  printf("*** FIN MKSTR S4 ***\n");

  printf("*****************\n");
  printf("usedsize: %zu\n", str_usedsize());
  printf("livesize: %zu\n", livesize);
  printf("freesize: %zu\n", str_freesize());
  printf("*****************\n");

  printf("\n*** DEBUT MKSTR S5 ***\n");
  String *s5 = mkstr("");
  printf("\nS5: %s\n", str_data(s5));
  printf("*** FIN MKSTR S5 ***\n");

  printf("*****************\n");
  printf("usedsize: %zu\n", str_usedsize());
  printf("livesize: %zu\n", livesize);
  printf("freesize: %zu\n", str_freesize());
  printf("*****************\n");


  printf("\n*** DEBUT FREE S2 ***\n");
  printf("\nS2: %s\n", str_data(s2));
  str_free(s2);
  printf("*** FIN FREE S2 ***\n");

  printf("*****************\n");
  printf("usedsize: %zu\n", str_usedsize());
  printf("livesize: %zu\n", livesize);
  printf("freesize: %zu\n", str_freesize());
  printf("*****************\n");

  printf("\n*** DEBUT FREE S1 ***\n");
  printf("\nS1: %s\n", str_data(s1));
  str_free(s1);
  printf("*** FIN FREE S1 ***\n");

  printf("*****************\n");
  printf("usedsize: %zu\n", str_usedsize());
  printf("livesize: %zu\n", livesize);
  printf("freesize: %zu\n", str_freesize());
  printf("*****************\n");

  printf("\n*** DEBUT FREE S5 ***\n");
  printf("\nS5: %s\n", str_data(s5));
  str_free(s5);
  printf("*** FIN FREE S5 ***\n");

  printf("*****************\n");
  printf("usedsize: %zu\n", str_usedsize());
  printf("livesize: %zu\n", livesize);
  printf("freesize: %zu\n", str_freesize());
  printf("*****************\n");

  printf("\n*** DEBUT FREE S3 ***\n");
  printf("\nS3: %s\n", str_data(s3));
  str_free(s3);
  printf("*** FIN FREE S3 ***\n");

  printf("*****************\n");
  printf("usedsize: %zu\n", str_usedsize());
  printf("livesize: %zu\n", livesize);
  printf("freesize: %zu\n", str_freesize());
  printf("*****************\n");

  printf("\n*** DEBUT FREE S4 ***\n");
  printf("\nS4: %s\n", str_data(s4));
  str_free(s4);
  printf("*** FIN FREE S4 ***\n");

  printf("*****************\n");
  printf("usedsize: %zu\n", str_usedsize());
  printf("livesize: %zu\n", livesize);
  printf("freesize: %zu\n", str_freesize());
  printf("*****************\n");

  printf("\n*** Debut freelist ***\n");
  String* freelist_node = freelist_head;
  while (freelist_node) {
    printf("Freelist node of size: %zu\n", freelist_node->dataslot_size);
    freelist_node = freelist_node->nextfree;
  }
  printf("\n*** Fin freelist ***\n");

  printf("\n*** DEBUT MKSTR S2 ***\n");
  s2 = mkstr ("test");
  printf("\nS2: %s\n", str_data(s2));
  printf("*** FIN MKSTR S2 ***\n");

  printf("*****************\n");
  printf("usedsize: %zu\n", str_usedsize());
  printf("livesize: %zu\n", livesize);
  printf("freesize: %zu\n", str_freesize());
  printf("*****************\n");

  printf("\n*** DEBUT MKSTR S1 ***\n");
  s1 = mkstr ("tt");
  printf("\nS1: %s\n", str_data(s1));
  printf("*** FIN MKSTR S1 ***\n");

  printf("*****************\n");
  printf("usedsize: %zu\n", str_usedsize());
  printf("livesize: %zu\n", livesize);
  printf("freesize: %zu\n", str_freesize());
  printf("*****************\n");

  printf("\n*** DEBUT MKSTR S3 ***\n");
  s3 = mkstr ("");
  printf("\nS3: %s\n", str_data(s3));
  printf("*** FIN MKSTR S3 ***\n");

  printf("*****************\n");
  printf("usedsize: %zu\n", str_usedsize());
  printf("livesize: %zu\n", livesize);
  printf("freesize: %zu\n", str_freesize());
  printf("*****************\n");

  printf("\n*** Debut freelist ***\n");
  freelist_node = freelist_head;
  while (freelist_node) {
    printf("Freelist node of size: %zu\n", freelist_node->dataslot_size);
    freelist_node = freelist_node->nextfree;
  }
  printf("\n*** Fin freelist ***\n");

  printf("\nS1: %s\n", str_data(s1));
  printf("\nS2: %s\n", str_data(s2));
  printf("\nS3: %s\n", str_data(s3));

  printf("\n*** DEBUT STR COMPACT ***\n");
  str_compact();
  printf("*** FIN STR COMPACT ***\n\n");

  printf("\nS1: %s\n", str_data(s1));
  printf("\nS2: %s\n", str_data(s2));
  printf("\nS3: %s\n", str_data(s3));

  printf("\n*** Debut freelist ***\n");
  freelist_node = freelist_head;
  while (freelist_node) {
    printf("Freelist node of size: %zu\n", freelist_node->dataslot_size);
    freelist_node = freelist_node->nextfree;
  }
  printf("\n*** Fin freelist ***\n");

  printf("\n*** DEBUT MKSTR S4 ***\n");
  s4 = mkstr ("tototo");
  printf("\nS4: %s\n", str_data(s4));
  printf("*** FIN MKSTR S4 ***\n");

  printf("\n*** DEBUT MKSTR S5 ***\n");
  s5 = mkstr ("totototatatatata");
  printf("\nS5: %s\n", str_data(s5));
  printf("*** FIN MKSTR S5 ***\n");

  printf("\n*** Debut freelist ***\n");
  freelist_node = freelist_head;
  while (freelist_node) {
    printf("Freelist node of size: %zu\n", freelist_node->dataslot_size);
    freelist_node = freelist_node->nextfree;
  }
  printf("\n*** Fin freelist ***\n");

  /* for (int i = 0; i < 500; i++) { */
  /* printf("\n*** DEBUT CONCAT i = %d ***\n", i); */
  /*   String *s4 = mkstr(str_data(s3)); */
  /* printf("\nS4: %s\n", str_data(s4)); */
  /* printf("*** FIN CONCAT i = %d ***\n", i); */
  /* } */

  /* for (int i = 0; i < 20; i++) { */
  /*   String *s4 = str_concat (s3, s3); */
  /*   str_free (s3); */
  /*   s3 = s4; */
  /* } */
}
