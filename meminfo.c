// Standard set of includes for an extension, plus a few extra.

#include "postgres.h"
#include "utils/builtins.h"
#include "utils/elog.h"
#include "funcapi.h"
#include "utils/backend_status.h"
#include "catalog/pg_authid.h"
#include "utils/acl.h"

// Our own libraries come next.

#include "hash.h"
#include "meminfo.h"

// Why enums? Because this allows our enumerated fields to closely match the
// smap fields in a very obvious way. If they add more in the future, the
// field values we've chosen will remain unaffected. If some are removed,
// they'll just show up as 0 in the column values.

typedef enum {
  SMAP_PID,
  SMAP_START_ADDRESS,
  SMAP_END_ADDRESS,
  SMAP_PERMISSIONS,
  SMAP_OFFSET,
  SMAP_DEV_MAJOR,
  SMAP_DEV_MINOR,
  SMAP_INODE,
  SMAP_PATH,
  SMAP_SIZE,
  SMAP_KERNEL_PAGE_SIZE,
  SMAP_MMU_PAGE_SIZE,
  SMAP_RSS,
  SMAP_PSS,
  SMAP_PSS_DIRTY,
  SMAP_SHARED_CLEAN,
  SMAP_SHARED_DIRTY,
  SMAP_PRIVATE_CLEAN,
  SMAP_PRIVATE_DIRTY,
  SMAP_REFERENCED,
  SMAP_ANONYMOUS,
  SMAP_KSM,
  SMAP_LAZY_FREE,
  SMAP_ANON_HUGE_PAGES,
  SMAP_SHMEM_PMD_MAPPED,
  SMAP_FILE_PMD_MAPPED,
  SMAP_SHARED_HUGE_TLB,
  SMAP_PRIVATE_HUGE_TLB,
  SMAP_SWAP,
  SMAP_SWAP_PSS,
  SMAP_LOCKED,
  SMAP_THP_ELIGIBLE,
  SMAP_VM_FLAGS,
  SMAP_FIELD_COUNT
} smap_fields;

// This is a list of all the currently known field labels in a proc smap
// formatted file. The locations supplied by the smap_fields enum ensure
// we can construct a consistent tuplestore based on the intended return col.

typedef struct smap_decode {
  char* key;
  uint32_t value;
} smap_decode;

static const smap_decode smap_mappings[] = {
  { "Size", SMAP_SIZE },
  { "KernelPageSize", SMAP_KERNEL_PAGE_SIZE },
  { "MMUPageSize", SMAP_MMU_PAGE_SIZE },
  { "Rss", SMAP_RSS },
  { "Pss", SMAP_PSS },
  { "Pss_Dirty", SMAP_PSS_DIRTY },
  { "Shared_Clean", SMAP_SHARED_CLEAN },
  { "Shared_Dirty", SMAP_SHARED_DIRTY },
  { "Private_Clean", SMAP_PRIVATE_CLEAN },
  { "Private_Dirty", SMAP_PRIVATE_DIRTY },
  { "Referenced", SMAP_REFERENCED },
  { "Anonymous", SMAP_ANONYMOUS },
  { "KSM", SMAP_KSM },
  { "LazyFree", SMAP_LAZY_FREE },
  { "AnonHugePages", SMAP_ANON_HUGE_PAGES },
  { "ShmemPmdMapped", SMAP_SHMEM_PMD_MAPPED },
  { "FilePmdMapped", SMAP_FILE_PMD_MAPPED },
  { "Shared_Hugetlb", SMAP_SHARED_HUGE_TLB },
  { "Private_Hugetlb", SMAP_PRIVATE_HUGE_TLB },
  { "Swap", SMAP_SWAP },
  { "SwapPss", SMAP_SWAP_PSS },
  { "Locked", SMAP_LOCKED },
  { "THPeligible", SMAP_THP_ELIGIBLE },
  { "VmFlags", SMAP_VM_FLAGS }
};

// Start the Postgres module and register any functions

PG_MODULE_MAGIC;
PG_FUNCTION_INFO_V1(get_smaps);

/**
 * Return the content of /proc/[pid]/smaps for every Postgres backend
 */
Datum
get_smaps(PG_FUNCTION_ARGS)
{
  ReturnSetInfo *rsinfo = (ReturnSetInfo *)fcinfo->resultinfo;
  Oid userid = GetUserId();

  int backend;
  int num_backends = pgstat_fetch_stat_numbackends();
  hash_item *hash_map;

  // As far as I know, the /proc/ filesystem only exists on Linux. If this
  // changes in the future, compatibility will too.

  #ifndef __linux__
    ereport(ERROR, (errcode(ERRCODE_FEATURE_NOT_SUPPORTED),
        errmsg("This function only works on Linux systems.")));
  #endif

  // Only allow superusers or users with pg_read_all_stats to use this, since
  // it technically leaks system information.

  if (!has_privs_of_role(userid, ROLE_PG_READ_ALL_STATS)) {
    ereport(ERROR, (errcode(ERRCODE_INSUFFICIENT_PRIVILEGE),
        errmsg("Must be superuser or member of pg_read_all_stats.")));
    PG_RETURN_VOID();
  }

  // This function is a godsend. Seriously. Just look up the function in
  // src/backend/utils/fmgr/funcapi.c and cry. That's _all_ boilerplate
  // you'd normally have to use. And it's _completely_ undocumented.

  InitMaterializedSRF(fcinfo, 0);

  // Rather than use a strcmp ladder 20+ elements long, build a hash lookup
  // table of all known fields in the smap format that we're parsing.

  if (!hash_map_create(&hash_map)) {
    ereport(ERROR, (errcode(ERRCODE_OUT_OF_MEMORY),
        errmsg("Could not allocate symbol lookup table.")));
    PG_RETURN_VOID();
  }

  for (uint32_t i = 0; i < sizeof(smap_mappings) / sizeof(smap_decode); i++)
    hash_insert(hash_map, smap_mappings[i].key, smap_mappings[i].value);

  // Loop through every backend process in the current instance. Using its pid,
  // find the /proc/[pid]/smaps file and begin parsing. Each file is read in
  // full, including fields for all address range headers. This will usually
  // be summarized, but is provided for deeper forensic purposes.

  for (backend = 1; backend <= num_backends; backend++)
  {
    // Just for the record, there are no nulls. Any missing value should just
    // be assumed to be zero.

    Datum values[SMAP_FIELD_COUNT] = {0};
    bool nulls[SMAP_FIELD_COUNT] = {0};

    char proc_path[256] = {0};
    FILE *fp;

    bool in_region = false;
    char line[512] = {0};

    // This seems like a lot of work just to get the PID of the target
    // backend. They really nested that bad boy _deep_.

    LocalPgBackendStatus *entry = pgstat_get_local_beentry_by_index(backend);
    values[SMAP_PID] = Int32GetDatum(entry->backendStatus.st_procpid);

    // We may not have been able to open the smap file, but this might not be
    // an error if the thread exited before we got to it. Just skip.

    sprintf(proc_path, "/proc/%d/smaps", entry->backendStatus.st_procpid);
    fp = fopen(proc_path, "r");

    if (fp == NULL)
      continue;

    // This is a very basic "regional" parser. Every time we see a header
    // row, we parse the header and don't try again until we've reached the
    // last non-header row. This process repeats until we reach the end of
    // the file or run out of headers to parse.

    while (fgets(line, sizeof(line), fp)) {

      char key[21] = {0};
      char sval[33] = {0};
      int32_t val = 0;

      if (strlen(line) < 1)
        break;

      // This is a very basic "regional" parser. Every time we see a header
      // row, we parse the header and don't try again until we've reached the
      // last non-header row. This process repeats until we reach the end of
      // the file or run out of headers to parse.

      if (!in_region) {
        char start_addr[33] = {0};
        char end_addr[33] = {0};
        char perms[5] = {0};
        char offset[13] = {0};
        char dev_major[3] = {0};
        char dev_minor[3] = {0};
        int32_t inode = 0;
        char path[256] = {0};

        int matched = sscanf(line, "%32[0-9a-f]-%32[0-9a-f] %4[rwxsp-] %12[0-9a-f] %2[0-9a-f]:%2[0-9a-f] %d %255s",
          start_addr, end_addr, perms, offset,
          dev_major, dev_minor, &inode, path
        );

        if (!matched)
          break;

        in_region = true;
        values[SMAP_START_ADDRESS] = CStringGetTextDatum(start_addr),
        values[SMAP_END_ADDRESS] = CStringGetTextDatum(end_addr);
        values[SMAP_PERMISSIONS] = CStringGetTextDatum(perms);
        values[SMAP_OFFSET] = CStringGetTextDatum(offset);
        values[SMAP_DEV_MAJOR] = CStringGetTextDatum(dev_major);
        values[SMAP_DEV_MINOR] = CStringGetTextDatum(dev_minor);
        values[SMAP_INODE] = Int32GetDatum(inode);
        values[SMAP_PATH] = CStringGetTextDatum(path);
        continue;
      }

      // After the header, every field is a basic "Key: val" format, except for
      // VmFlags, which is a space-delimited string of two-letter flags.

      if (sscanf(line, "%20[a-zA-Z_]: %d", key, &val) == 2) {
        int32_t hash_index = hash_lookup(hash_map, key);
        if (hash_index > 0)
          values[hash_index] = Int32GetDatum(val);
      }
      else if (sscanf(line, "%20[a-zA-Z_]: %32[a-z ]", key, sval) == 2) {
        int32_t hash_index = hash_lookup(hash_map, key);
        if (hash_index > 0)
          values[hash_index] = CStringGetTextDatum(sval);
      }

      // The VmFlags field is the last in the current address range. Commit
      // this tuple and move on to the next.

      if (strcmp(key, "VmFlags") == 0) {
        in_region = false;
        tuplestore_putvalues(rsinfo->setResult, rsinfo->setDesc, values, nulls);
      }

    } // End smap pid loop

    fclose(fp);

  } // End backend loop

  PG_RETURN_VOID();

} // get_smaps
