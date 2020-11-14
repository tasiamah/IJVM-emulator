#include <ijvm.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <limits.h> 
#include "stack.h"

#define OBJREF 0x11
#define MAX_ELEMENTS 1000

FILE *outfile;
FILE *infile;
static byte_t *byteArray;
static byte_t *textArray;
static word_t *constantArray;
static uint32_t textsize;
static uint32_t constantsize;
static ijvmstack_t *stack;
struct Frame *frame;
static int programCounter = 0;
static bool isFinished = false;
struct Frame *head;
struct Frame *tail;
bool wide_command;

typedef struct Frame{
    int programCounter;
    int oldsp;
    word_t *frame;
    int frameTop;
    
    struct Frame *next;
    struct Frame *prev;

} Frame_t;

Frame_t *add_frame() {
    Frame_t *new_frame = malloc(sizeof(Frame_t));
    if (!head) {
        head = new_frame;
        tail = new_frame;
    }
    else {
        new_frame->prev = tail;
        tail->next = new_frame;
        tail = new_frame;
    }
    new_frame->programCounter = 0;
    new_frame->oldsp = 0;
    new_frame->frameTop = 0;
    
    return new_frame;
}

void remove_current_frame() {
    Frame_t *temp = tail;
    if(tail && tail->prev) {
        tail->prev->next = NULL;
        tail = tail->prev;
    }
    free(temp->frame);
    free(temp);
}

void ireturn() {
    word_t ret = stack_pop(stack);
    
    programCounter = tail->programCounter;
    stack->top = tail->oldsp - 1;
    
    stack_push(stack, ret);
    printf("%d", tos());
    remove_current_frame();
}
 
uint16_t bytes_to_short(uint8_t first, uint8_t second) {
    return ((first << 8) & 0xff00) | (second & 0xff);
}

void invokevirtual() {
    uint8_t argcount;
    uint8_t varsize;
    Frame_t *newframe = add_frame();
    
    uint8_t fir, sec;
    fir = get_instruction();
    programCounter++;
    sec = get_instruction();
    programCounter++;
    
    uint16_t index = bytes_to_short(fir, sec);
    
    newframe->programCounter = programCounter;
    programCounter = get_constant(index);
    
    fir = get_instruction();
    programCounter++;
    sec = get_instruction();
    programCounter++;
    argcount = bytes_to_short(fir, sec);
    
    fir = get_instruction();
    programCounter++;
    sec = get_instruction();
    programCounter++;
    varsize = bytes_to_short(fir, sec);
    
    newframe->frame = malloc((argcount + varsize + 1) * sizeof(word_t));
    if(!(newframe->frame)) exit(-5);
    
    for(int i = 0; i < argcount; i++) {
        newframe->frame[argcount - i - 1] = stack_pop(stack);
    }
    stack_push(stack, OBJREF);
    newframe->oldsp = stack->top;
}

void bipush() {
    int8_t argument = get_instruction();
    programCounter++;
    stack_push(stack, argument);
}

void iadd() {
    word_t fir = stack_pop(stack);
    word_t sec = stack_pop(stack);
    stack_push(stack, fir + sec);
}

void isub() {
    word_t fir = stack_pop(stack);
    word_t sec = stack_pop(stack);
    stack_push(stack, sec - fir);
}

void iand() {
    word_t fir = stack_pop(stack);
    word_t sec = stack_pop(stack);
    stack_push(stack, fir & sec);
}

void ior() {
    word_t fir = stack_pop(stack);
    word_t sec = stack_pop(stack);
    stack_push(stack, fir | sec);
}

void swap() {
    word_t fir = stack_pop(stack);
    word_t sec = stack_pop(stack);
    stack_push(stack, fir);
    stack_push(stack, sec);

}

void dupl(){
    stack_push(stack, stack_peek(stack));
}

void go_to() {
    byte_t fir = get_instruction();
    programCounter++;
    byte_t sec = get_instruction();
    programCounter++;
    int16_t argument = ((fir << 8) & 0xff00) | (sec & 0xff);
    programCounter += (int) argument - 3;
}

void ifeq() {
    if (stack_pop(stack) == 0) {
        byte_t fir = get_instruction();
        programCounter++;
        byte_t sec = get_instruction();
        programCounter++;
        int16_t argument = ((fir << 8) & 0xff00) | (sec & 0xff);
        programCounter += (int) argument - 3;
    } else {
        programCounter += 2;
    }
}

void iflt() {
    if (stack_pop(stack) < 0) {
        byte_t fir = get_instruction();
        programCounter++;
        byte_t sec = get_instruction();
        programCounter++;
        int16_t argument = ((fir << 8) & 0xff00) | (sec & 0xff);
        programCounter += (int) argument - 3;
    } else {
        programCounter += 2;
    }

}

void icmpeq() {
    word_t fir =stack_pop(stack);
    word_t sec =stack_pop(stack);
    if (fir == sec) {
        byte_t fir = get_instruction();
        programCounter++;
        byte_t sec = get_instruction();
        programCounter++;
        int16_t argument = ((fir << 8) & 0xff00) | (sec & 0xff);
        programCounter += (int) argument - 3;
    } else {
        programCounter += 2;
    }
}

void ldc_w(){
        byte_t fir = get_instruction();
        programCounter++;
        byte_t sec = get_instruction();
        programCounter++;
        uint16_t index = ((fir << 8) & 0xff00) | (sec & 0xff);
    	stack_push(stack, constantArray[index]);
        printf("%d\n", stack_peek(stack));
}

void istore(){
    int index;
    if(wide_command) {
        int fir = get_instruction();
        programCounter++;
        int sec = get_instruction();
        programCounter++;
        
        index = bytes_to_short(fir, sec);
    }
    else {
    	index = (int) get_instruction();
        programCounter++;        
    }
    word_t data = stack_pop(stack);
    if(tail) tail->frame[index] = data;
    printf("%d\n", tail->frame[index]);
}

void iload() {
    int index;
    
    if(wide_command) {
        int fir = get_instruction();
        programCounter++;
        int sec = get_instruction();
        programCounter++;
        
        index = bytes_to_short(fir, sec);
    }
    else {
        index = get_instruction();
        programCounter++;
    }
    if(stack && tail) stack_push(stack, tail->frame[index]);
}

void iinc(){
    int index;
    
    if(wide_command) {
        int fir = get_instruction();
        programCounter++;
        int sec = get_instruction();
        programCounter++;
        
        index = bytes_to_short(fir, sec);
    }
    else {
        index = get_instruction();
        programCounter++;
    }
    int8_t constant = (int8_t) get_instruction();
    programCounter++;
    frame->frame[index] += constant;
    printf("%d\n", frame->frame[index]);
}

void wide(){
    wide_command = true;
}

void print_out() {
    char outpt = (char) stack_pop(stack);
    if(outfile != NULL)
{
    fprintf(outfile, "%c", outpt);
}
    printf("%c\n", outpt);
}

void in() {
    char inchar = fgetc(infile);
    if(inchar == EOF) stack_push(stack, 0);
    else stack_push(stack, inchar);
}

void destroy_ijvm() {
    free(byteArray);
    free(textArray);
    free(constantArray);
    isFinished = false;
    textsize = 0;
    programCounter = 0;
}

void set_input(FILE *fp) {
    infile = fp;
}

void set_output(FILE *fp) {
    outfile = fp;
}

word_t tos(void) {
    word_t result = stack_peek(stack);
    return result;
}

word_t *get_stack(void) {
    return stack->array;
}

int stack_size(void) {
    return stack->top;
}

byte_t *get_text(void) {
    return textArray;
}

int text_size(void) {
    return textsize;
}

int get_program_counter(void) {
    return programCounter;
}

word_t get_local_variable(int i) {
    return tail->frame[i];
}

word_t get_constant(int i) {
    return constantArray[i];
}

bool finished(void) {
    return isFinished;
}

byte_t get_instruction(void) {
    int8_t instruction = textArray[programCounter];
    return instruction;
}

bool step(void) {
    if (programCounter == textsize) {
        isFinished = true;
        return false;
    }
    byte_t instruction = get_instruction();
    programCounter++;

    switch (instruction) {

        case OP_NOP:
            break;
            
        case OP_BIPUSH:
            bipush();
            break;
        case OP_DUP:
            dupl();
            break;
        case OP_POP:
           stack_pop(stack);
            break;
        case OP_SWAP:
            swap();
            break;
        case OP_IADD:
            iadd();
            break;
        case OP_ISUB:
            isub();
            break;
        case OP_IAND:
            iand();
            break;
        case OP_IOR:
            ior();
            break;
        case OP_GOTO:
            go_to();
            break;
        case OP_IFEQ:
            ifeq();
            break;
        case OP_IFLT:
            iflt();
            break;
        case OP_ICMPEQ:
            icmpeq();
            break;
        case OP_LDC_W:
            ldc_w();
            break;
        case OP_ISTORE:
            istore();
            break;
        case OP_ILOAD:
            iload();
            break;
        case OP_IINC:
            iinc();
            break;
        case OP_WIDE:
            wide();
            step();
            break;
        case OP_INVOKEVIRTUAL:
            invokevirtual();
            break;
        case OP_IRETURN:
            ireturn();
            break;
        case OP_IN:
            in();
            break;  
        case OP_OUT:
            print_out();
            break;
        case OP_HALT:
            programCounter = textsize;
            break;
        case OP_ERR:
            exit(0);
            break;

        default:
            printf("instruction unrecognized\n");
            break;
    }
    return true;
}

void run() {
    while (!isFinished) {
        step();
    }
}

int getInput(char* binary_file) {
    FILE *fp;
    fp = fopen(binary_file, "rb");
    int result = fread(byteArray, sizeof (char), 1000, fp);
    fclose(fp);
    return result;
}

void initConstants(byte_t* byteArray){
    int constantPointer = 0;
    constantsize = byteArray[11] | (uint32_t) byteArray[10] << 8
            | (uint32_t) byteArray[9] << 16 | (uint32_t) byteArray[8] << 24;
    
    for(int i = 12;i < 12+constantsize;i+=4){
       word_t constant = byteArray[i+3] | (uint32_t) byteArray[i+2] << 8
            | (uint32_t) byteArray[i+1] << 16 | (uint32_t) byteArray[i] << 24;
       constantArray[constantPointer] = constant;
       constantPointer++;
    }
}

void initText(byte_t* byteArray) {
    int textPointer = 0;
    constantsize = byteArray[11] | (uint32_t) byteArray[10] << 8
            | (uint32_t) byteArray[9] << 16 | (uint32_t) byteArray[8] << 24;

    textsize = byteArray[11 + constantsize + 8] | (uint32_t) byteArray[11 + constantsize + 7] << 8
            | (uint32_t) byteArray[11 + constantsize + 6] << 16 | (uint32_t) byteArray[11 + constantsize + 5] << 24;

    for (int i = 20 + constantsize; i < 20 + constantsize + textsize; i++) {
        textArray[textPointer] = byteArray[i];
        textPointer++;
    }
}

int init_ijvm(char *binary_file) {
    outfile = stdout;
    infile = stdin;
    byteArray = malloc(MAX_ELEMENTS * sizeof (byte_t));
    textArray = malloc(MAX_ELEMENTS * sizeof (byte_t));
    constantArray = malloc(MAX_ELEMENTS * sizeof (word_t));
    stack = stack_new(MAX_ELEMENTS);
    frame = add_frame();
    frame->frame = malloc(MAX_ELEMENTS * sizeof(word_t));

    int value = getInput(binary_file);
    initText(byteArray);
    initConstants(byteArray);
    wide_command = false;

    return value;
}