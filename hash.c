#include "postgres.h"
#include "hash.h"

/**
 * Allocate memory for the hash map using Postgres palloc0
 */
int hash_map_create(hash_item **hash_map) {
  *hash_map = (hash_item *)palloc0(HASH_TABLE_SIZE * sizeof(hash_item));
  return (*hash_map == NULL) ? 0 : 1;
} // hash_map_create

/**
 * Hash any string passed to this function
 * 
 * This is just the djb2 hash function since it's quick and easy. We only
 * need to _reduce_ collisions during bucket storage, not eliminate them.
 */
uint32_t hash_str(const char* s) {
  uint32_t hash = 5381;

  while (*s)
    hash = ((hash << 5) + hash) + *s++;

  return hash;
} // hash_str

/**
 * Look up a potentially hashed string and return the stored value
 * 
 * Stored values are always unsigned integers because that's all the related
 * meminfo extension needs.
 */
uint32_t hash_lookup(hash_item *map, const char *s) {
  uint32_t hash = hash_str(s);
  uint32_t bucket = hash % HASH_TABLE_SIZE;
  uint32_t tries = 0;

  // If the first bucket doesn't match, search up to half of the hash table
  // using a linear probe which can wrap around to the beginning. This is the
  // same procedure used to store the values and keeps the search fast.

  while (map[bucket].hash != hash) {
    if (++bucket == HASH_TABLE_SIZE)
      bucket = 0;
    if (++tries == HASH_TABLE_SIZE / 2)
      break;
  }

  if (tries < HASH_TABLE_SIZE)
    return map[bucket].value;

  return 0;
} // hash_lookup


/**
 * Store a string hash and the associated value
 * 
 * By storing the hash itself, lookups can use the same process to compare
 * their hash to the contents of each bucket in the hash table.
 */
int hash_insert(hash_item *map, const char *s, uint32_t value) {
  uint32_t hash = hash_str(s);
  uint32_t bucket = hash % HASH_TABLE_SIZE;
  uint32_t tries = 0;

  // Start at the calculated bucket location. Since no raw hash can be zero,
  // we can easily look for an empty location using a linear probe. Only try
  // for half of the hash table before giving up. This should never happen
  // for our tiny use case.

  while (map[bucket].hash > 0) {
    if (++bucket == HASH_TABLE_SIZE)
      bucket = 0;
    if (++tries == HASH_TABLE_SIZE / 2)
      break;
  }

  if (tries < HASH_TABLE_SIZE) {
    map[bucket].hash = hash;
    map[bucket].value = value;
    return 1;
  }

  return 0;
} // hash_insert
