#include <stdlib.h>
#include <assert.h>
#include "stack.h"
typedef xc_stack_t* S;

void xc_stack_init(S stack)
{
	stack->cnt = 0;
	stack->size = 8;
	stack->data = malloc(sizeof(void *) * stack->size);
}

void xc_stack_destroy(S stack)
{
	free(stack->data);
}

void xc_stack_push(S stack, void *item)
{
	if (stack->cnt == stack->size) {
		stack->size <<= 1;
		stack->data = realloc(stack->data, sizeof(void *) * stack->size);
	}
	stack->data[stack->cnt++] = item;
}

void* xc_stack_pop(S stack)
{
	assert(stack != NULL && stack->size > 0);
	return stack->data[--stack->cnt];
}

void* xc_stack_top(S stack)
{
	assert(stack != NULL && stack->cnt > 0);
	return stack->data[stack->cnt-1];
}

void* xc_stack_get(S stack, int n)
{
	assert(stack != NULL && stack->cnt > 0);
	return stack->data[n];
}

int xc_stack_size(S stack)
{
	assert(stack != NULL);
	return stack->cnt;
}

void xc_stack_reverse(S stack)
{
	int i, j;
	void *tmp;

	assert(stack != NULL);
	for (i = 0, j = stack->cnt - 1; i < j; i ++, j --) {
		tmp = stack->data[i];
		stack->data[i] = stack->data[j];
		stack->data[j] = tmp;
	}
}
