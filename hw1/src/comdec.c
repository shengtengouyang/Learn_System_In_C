#include "const.h"
#include "sequitur.h"
#include "debug.h"

#ifdef _STRING_H
#error "Do not #include <string.h>. You will get a ZERO."
#endif

#ifdef _STRINGS_H
#error "Do not #include <strings.h>. You will get a ZERO."
#endif

#ifdef _CTYPE_H
#error "Do not #include <ctype.h>. You will get a ZERO."
#endif

/*
 * You may modify this file and/or move the functions contained here
 * to other source files (except for main.c) as you wish.
 *
 * IMPORTANT: You MAY NOT use any array brackets (i.e. [ and ]) and
 * you MAY NOT declare any arrays or allocate any storage with malloc().
 * The purpose of this restriction is to force you to use pointers.
 * Variables to hold the pathname of the current file or directory
 * as well as other data have been pre-declared for you in const.h.
 * You must use those variables, rather than declaring your own.
 * IF YOU VIOLATE THIS RESTRICTION, YOU WILL GET A ZERO!
 *
 * IMPORTANT: You MAY NOT use floating point arithmetic or declare
 * any "float" or "double" variables.  IF YOU VIOLATE THIS RESTRICTION,
 * YOU WILL GET A ZERO!
 */

/**
 * Main compression function.
 * Reads a sequence of bytes from a specified input stream, segments the
 * input data into blocks of a specified maximum number of bytes,
 * uses the Sequitur algorithm to compress each block of input to a list
 * of rules, and outputs the resulting compressed data transmission to a
 * specified output stream in the format detailed in the header files and
 * assignment handout.  The output stream is flushed once the transmission
 * is complete.
 *
 * The maximum number of bytes of uncompressed data represented by each
 * block of the compressed transmission is limited to the specified value
 * "bsize".  Each compressed block except for the last one represents exactly
 * "bsize" bytes of uncompressed data and the last compressed block represents
 * at most "bsize" bytes.
 *
 * @param in  The stream from which input is to be read.
 * @param out  The stream to which the block is to be written.
 * @param bsize  The maximum number of bytes read per block.
 * @return  The number of bytes written, in case of success,
 * otherwise EOF.
 */
int compress(FILE *in, FILE *out, int bsize) {
    // To be implemented.
    return EOF;
}

int numBytes(int character){
    if((character&0xF8)==0xF0)
        return 4;
    else if((character&0xF0)==0xE0)
        return 3;
    else if((character&0xE0)==0xC0)
        return 2;
    else if ((character&0x80)==0)
        return 1;
    return 0;
}
int converLaterBytes(FILE *in, int num){
    int result=0;
    while(num>1){
        int b=fgetc(in);
        if (b==EOF){
            return -1;
        }
        if ((b&0XC0)==0x80){
            b=b^0x80;
            result<<=6;
            result+=b;
            num--;
        }
        else{
            return -1;
        }
    }
    return result;
}

int convert(FILE *in, int b1){
    int num=numBytes(b1);
    int blater;
    switch (num){
        case 1:
            return b1;
        case 2:
            b1=b1^0xC0;
            break;
        case 3:
            b1=b1^0xE0;
            break;
        case 4:
            b1=b1^0xF0;
            break;
        default:
            return -1;
    }
    blater=converLaterBytes(in, num);
    if(blater==-1){
        return -1;
    }
    return (b1<<6)+blater;
}
void add_symbol(SYMBOL *symbol, SYMBOL *currentRule){
    SYMBOL *temp;
    temp=currentRule->prev;
    currentRule->prev=symbol;
    temp->next=symbol;
    symbol->prev=temp;
    symbol->next=currentRule;
}

void expands(FILE *out, SYMBOL *currentRule){
    SYMBOL *currentSymbol=currentRule->next;
    while(currentSymbol!=currentRule){
        if(IS_NONTERMINAL(currentSymbol)){
            expands(out, currentSymbol->rule);
        }
        fputc(currentSymbol->value, out);
        currentSymbol=currentSymbol->next;
    }
}

/**
 * Main decompression function.
 * Reads a compressed data transmission from an input stream, expands it,
 * and and writes the resulting decompressed data to an output stream.
 * The output stream is flushed once writing is complete.
 *
 * @param in  The stream from which the compressed block is to be read.
 * @param out  The stream to which the uncompressed data is to be written.
 * @return  The number of bytes written, in case of success, otherwise EOF.
 */
int decompress(FILE *in, FILE *out) {
    int character;
    int num=0;
    int rulePosition=0;
    SYMBOL *currentRule;
    while((character=fgetc(in))!=EOF){
        if(character==0x81||character==0x83||character==0x85){
            continue;
        }
        else if(character==0x82){
            break;
        }
        else if(character==0x84){
            init_symbols();
            init_rules();
        }
        else if(character==0x85){
            rulePosition=0;
        }
        //wait for check if marker needs to be checked
        else{
            character=convert(in, character);
            if(character==-1){
                return EOF;
            }
            else{
                if(rulePosition==0){
                    if(character<FIRST_NONTERMINAL){
                        return EOF;
                    }
                    else{
                        currentRule=new_rule(character);
                        add_rule(currentRule);
                        rulePosition++;
                    }
                }
                else{
                    add_symbol(new_symbol(character, NULL), currentRule);
                }
                num++;
            }
        }
    }
    expands(out, main_rule);
    // To be implemented.
    return num;
}

/**
 * @brief Validates command line arguments passed to the program.
 * @details This function will validate all the arguments passed to the
 * program, returning 0 if validation succeeds and -1 if validation fails.
 * Upon successful return, the selected program options will be set in the
 * global variable "global_options", where they will be accessible
 * elsewhere in the program.
 *
 * @param argc The number of arguments passed to the program from the CLI.
 * @param argv The argument strings passed to the program from the CLI.
 * @return 0 if validation succeeds and -1 if validation fails.
 * Refer to the homework document for the effects of this function on
 * global variables.
 * @modifies global variable "global_options" to contain a bitmap representing
 * the selected options.
 */
int validargs(int argc, char **argv)
{
    if(argc<=1){
        return -1;
    }
    char *temp=*(argv+1);
    if(*temp=='-'){
        temp++;
        if(*(temp+1)=='\0'){
            if(*temp=='h'){
                global_options=1;
                    printf("global is: %d\n", global_options);
                return 0;
            }
            else if(*temp=='c'){
                if(argc==2){
                    global_options=1024;
                    global_options=(global_options<<16)+2;
                    printf("global is: %d\n", global_options);
                    return 0;
                }
                else if(argc==4){
                    temp=*(argv+2);
                    if(*temp=='-'&&*(temp+1)=='b'&&*(temp+2)=='\0'){
                        unsigned x=0;
                        for(temp=*(argv+3);*temp!='\0';temp++){
                            if(*temp>='0'&&*temp<='9'){
                                x=x*10+(*temp-'0');
                            }
                            else{
                                return -1;
                            }
                        }
                        if(x>=1&&x<=1024){
                            x<<=16;
                            global_options=x+2;
                            printf("global is: %d x is: %d\n", global_options, x);
                            return 0;
                        }
                    }
                }
            }
            else if(*temp=='d'){
                if(argc==2){
                    global_options=4;
                    printf("global is: %d\n", global_options);
                    return 0;
                }
            }
        }
    }
    return -1;
}
