# The pg_meminfo Postgres Extension

Have you ever wanted to know how much RAM each individual Postgres backend is currently using? Maybe you've asked this question before, and stumbled across some arcane command-line incantation like this:

```sh
cat /proc/4242/smaps | grep Pss | awk '{total += $2} END {print total}'
```

Wouldn't you rather do it this way?

```sql
SELECT pss FROM smap_summary WHERE pid = 4242;
```

That's exactly what this extension provides: valuable diagnostic memory information for any backend in the current Postgres cluster, right when you need it.

## Installation

Installing this extension is simple:

```bash
git clone git@github.com:bonesmoses/pg_meminfo.git
cd pg_meminfo
make
sudo make install
```

Then connect to any database and execute this statement:

```sql
CREATE EXTENSION pg_meminfo;
```

## Usage

This extension currently only provides one view, and one function.

The `get_all_smaps()` function provides the PID of each backend along with statistics for every registered memory address range listed in `/proc/[pid]/smaps`. It can only be executed by superusers, or users who are members of the `pg_read_all_stats` role.

Each memory address header contributes the fields as described in the [proc_pid_maps](https://www.man7.org/linux/man-pages/man5/proc_pid_maps.5.html) documentation. The remaining fields are described in the [proc_pid_smaps](https://www.man7.org/linux/man-pages/man5/proc_pid_smaps.5.html) manual.

Since it's more common to aggregate the memory readings rather than studying each individual memory address range, this extension also supplies the `smap_summary` view. It removes the following fields which are part of the address range header:

- `start_address`
- `end_address`
- `permissions`
- `byte_offset`
- `dev_major`
- `dev_minor`
- `inode`
- `sys_path`

Then it aggregates the remaining columns as sums by PID.

Aside from the header columns, `thp_eligible`, and `vm_flags`, all other columns are expressed in kilobytes, as described by the smaps documentation.

## Discussion

This extension may act as a learning exercise or skeleton for writing Postgres extensions which do the following:

* Add functions which return records or sets (SRF functions).
* Check user role access.
* Examine Postgres backend status.
* Use the Postgres memory allocator.
* Multi-source builds.

Of particular note, this extension makes use of the `InitMaterializedSRF` function in [`utils/fmgr/funcapi.c`](https://github.com/postgres/postgres/blob/mastersrc/backend/utils/fmgr/funcapi.c), which _vastly_ reduces the amount of boilerplate code for set-returning functions.
