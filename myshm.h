typedef struct _xc_shm_t xc_shm_t;
typedef unsigned int xc_shmsize_t;

int xc_shm_can_readonly(xc_shm_t *shm);
int xc_shm_is_readwrite(xc_shm_t *shm, const void *p);
int xc_shm_is_readonly(xc_shm_t *shm, const void *p);
void *xc_shm_to_readwrite(xc_shm_t *shm, void *p);
void *xc_shm_to_readonly(xc_shm_t *shm, void *p);

void *xc_shm_ptr(xc_shm_t *shm);

xc_shm_t *xc_shm_init(const char *path, xc_shmsize_t size, zend_bool readonly_protection);
void xc_shm_destroy(xc_shm_t *shm);
