#include <ijvm.h>
#include <stdlib.h>
#include "stack.h"


ijvmstack_t *stack_new(unsigned capacity) {
    ijvmstack_t *stack = malloc(sizeof(ijvmstack_t));
    stack->capacity = capacity;
    stack->top = -1;
    stack->array = malloc(stack->capacity * sizeof (word_t));
    return stack;
}

 
void stack_push(ijvmstack_t* stack, word_t item) {
    if (stack->top == stack->capacity - 1)
        return;
    stack->top++;
    stack->array[stack->top] = item;
}

int stack_pop(ijvmstack_t* stack) {
    if (stack->top == -1)
        return -1;
    return stack->array[stack->top--];
}

int stack_peek(ijvmstack_t* stack) {
    if (stack->top == -1)
        return -1;
    return stack->array[stack->top];
}

