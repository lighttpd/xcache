#ifndef __XC_UTIL_STACK
#define __XC_UTIL_STACK

typedef struct {
	void **data;
	int cnt;
	int size;
} xc_stack_t;

#define S xc_stack_t*
void xc_stack_init_ex(S stack, int initsize);
#define xc_stack_init(stack) xc_stack_init_ex(stack, 8)
void xc_stack_destroy(S stack);
void xc_stack_push(S stack, void *item);
void *xc_stack_pop(S stack);
void *xc_stack_top(S stack);
void *xc_stack_get(S stack, int n);
int xc_stack_count(S stack);
void xc_stack_reverse(S stack);
#undef S

#endif /* __XC_UTIL_STACK */
