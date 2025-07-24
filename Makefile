MODULE_big = pg_meminfo

PGFILEDESC = "Extension to view Postgres related memory information"
EXTENSION = pg_meminfo
DATA = pg_meminfo--1.0.sql
OBJS = meminfo.o hash.o

PG_CONFIG = pg_config
PGXS := $(shell $(PG_CONFIG) --pgxs)
include $(PGXS)
