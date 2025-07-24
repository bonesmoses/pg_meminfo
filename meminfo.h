#ifndef PGMEMINFO_H
#define PGMEMINFO_H

#define PGMEMINFO_VERSION "1.0.0"
#define PGMEMINFO_VERSION_NUM 10000

/**
 * Return the content of /proc/[pid]/smaps for every Postgres backend
 * 
 * This function will iterate through every Postgres backend and parse the
 * /proc/[pid]/smaps file for all address ranges, returning one row per range.
 * Since these may disappear before being opened, the function treats file
 * errors as exited pids and will simply skip those backend results rather
 * than throw an error.
 * 
 * This function ONLY works on Linux systems!
 * 
 * Only runs as either a superuser or member of pg_read_all_stats.
 * 
 * See the documentation of proc_pid_smaps for more information:
 * 
 * https://www.man7.org/linux/man-pages/man5/proc_pid_smaps.5.html
 */
Datum get_smaps(PG_FUNCTION_ARGS);

#endif // PGMEMINFO_H
