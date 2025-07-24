/**
 * Extremely rudimentary hash table lookup system
 * 
 * This library exists primarily to store string -> int pairs in a fast hash
 * lookup system. It translates any string to a simple uint hash. This hash is
 * then used to assign an initial bucket location in the hash map based on the
 * maximum size of the map. If this position is in use, it uses linear probing
 * to try and find a new location, and will traverse up to half of the map size
 * to find one before giving up. Both the hash and value are then stored.
 * 
 * Lookups work the same way. Strings are translated into a hash and if the
 * hash of the expected bucket doesn't match, we traverse up to half of the map
 * to find the correct entry. Upon finding the entry for that hash, the value
 * is returned.
 * 
 * The intent is to use an accompanying enum to store expected array locations,
 * allowing translation of text labels directly into enum values without a
 * strcmp ladder.
 * 
 * THIS ONLY WORKS WITH Postgres since we use palloc0 rather than malloc!
 * 
 * Usage:
 * 
 * hash_item *hash_table;
 * hash_map_create(&hash_table);
 * hash_insert(hash_table, "meaning_of_life", 42);
 * hash_lookup(hash_table, "meaning_of_life");
 */

#ifndef PGMEMINFO_HASH_H
#define PGMEMINFO_HASH_H

#define HASH_TABLE_SIZE 64

typedef struct hash_item {
  uint32_t hash;
  uint32_t value;
} hash_item;

uint32_t hash_str(const char* s);
int hash_map_create(hash_item **hash_map);
int hash_insert(hash_item *map, const char *s, uint32_t value);
uint32_t hash_lookup(hash_item *map, const char *s);

#endif // PGMEMINFO_HASH_H
