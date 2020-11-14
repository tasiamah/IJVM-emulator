#ifndef STACK_H
#define STACK_H


typedef struct Stack {
    int top;
    unsigned capacity;
    word_t* array;

} Stack;

typedef struct Stack ijvmstack_t;

ijvmstack_t* stack_new(unsigned capacity);
 
void stack_push(ijvmstack_t* stack, word_t item);

int stack_pop(ijvmstack_t* stack);

int stack_peek(ijvmstack_t* stack);



#endif 

