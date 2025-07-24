\echo Use "CREATE EXTENSION pg_meminfo" to load this file. \quit

CREATE OR REPLACE FUNCTION get_all_smaps()
RETURNS TABLE(
  pid INT, 
  start_address TEXT, 
  end_address TEXT,
  permissions CHAR(4),
  byte_offset TEXT,
  dev_major CHAR(2),
  dev_minor CHAR(2),
  inode INT,
  sys_path TEXT,
  mem_size INT,
  kernel_page_size INT,
  mmu_page_size INT,
  rss INT,
  pss INT,
  pss_dirty INT,
  shared_clean INT,
  shared_dirty INT,
  private_clean INT,
  private_dirty INT,
  referenced INT,
  anonymous INT,
  ksm INT,
  lazy_free INT,
  anon_huge_pages INT,
  shmem_pmd_mapped INT,
  file_pmd_mapped INT,
  shared_huge_tlb INT,
  private_huge_tlb INT,
  swap INT,
  swap_pss INT,
  locked INT,
  thp_eligible INT,
  vm_flags TEXT
)
AS 'MODULE_PATHNAME', 'get_smaps'
LANGUAGE C VOLATILE;

CREATE OR REPLACE VIEW smap_summary AS
SELECT pid, 
       sum(mem_size) AS mem_size,
       sum(kernel_page_size) AS kernel_page_size,
       sum(mmu_page_size) AS mmu_page_size,
       sum(rss) AS rss,
       sum(pss) AS pss,
       sum(pss_dirty) AS pss_dirty,
       sum(shared_clean) AS shared_clean,
       sum(shared_dirty) AS shared_dirty,
       sum(private_clean) AS private_clean,
       sum(private_dirty) AS private_dirty,
       sum(referenced) AS referenced,
       sum(anonymous) AS anonymous,
       sum(ksm) AS ksm,
       sum(lazy_free) AS lazy_free,
       sum(anon_huge_pages) AS anon_huge_pages,
       sum(shmem_pmd_mapped) AS shmem_pmd_mapped,
       sum(file_pmd_mapped) AS file_pmd_mapped,
       sum(shared_huge_tlb) AS shared_huge_tlb,
       sum(private_huge_tlb) AS private_huge_tlb,
       sum(swap) AS swap,
       sum(swap_pss) AS swap_pss,
       sum(locked) AS locked,
       sum(thp_eligible) AS thp_eligible
  FROM get_all_smaps()
 GROUP BY pid;
