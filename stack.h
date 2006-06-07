
typedef struct {
	void **data;
	int cnt;
	int size;
} xc_stack_t;

#define S xc_stack_t*
void xc_stack_init(S stack);
void xc_stack_destroy(S stack);
void xc_stack_push(S stack, void *item);
void *xc_stack_pop(S stack);
void *xc_stack_top(S stack);
void *xc_stack_get(S stack, int n);
int xc_stack_size(S stack);
void xc_stack_reverse(S stack);
#undef S
