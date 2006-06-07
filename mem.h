typedef struct _xc_mem_t xc_mem_t;
typedef struct _xc_block_t xc_block_t;
typedef unsigned int xc_memsize_t;

void *xc_mem_malloc(xc_mem_t *mem, xc_memsize_t size);
int xc_mem_free(xc_mem_t *mem, const void *p);
void *xc_mem_calloc(xc_mem_t *mem, xc_memsize_t memb, xc_memsize_t size);
void *xc_mem_realloc(xc_mem_t *mem, const void *p, xc_memsize_t size);
char *xc_mem_strndup(xc_mem_t *mem, const char *str, xc_memsize_t len);
char *xc_mem_strdup(xc_mem_t *mem, const char *str);
const xc_block_t *xc_mem_freeblock_first(xc_mem_t *mem);
const xc_block_t *xc_mem_freeblock_next(const xc_block_t *block);
xc_memsize_t xc_mem_block_size(const xc_block_t *block);
xc_memsize_t xc_mem_block_offset(const xc_mem_t *mem, const xc_block_t *block);

xc_memsize_t xc_mem_avail(xc_mem_t *mem);
xc_memsize_t xc_mem_size(xc_mem_t *mem);

xc_mem_t *xc_mem_init(void *ptr, xc_memsize_t size);
void xc_mem_destroy(xc_mem_t *mem);
