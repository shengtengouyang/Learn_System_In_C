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
int convert_utf8(int character, FILE *out, int num){
    int bytes;
    if(character<=0x7F){
        bytes=1;
    }
    else if(character<=0x7FF){
        bytes=2;
    }
    else if(character<=0xFFFF){
        bytes=3;
    }
    else if(character<=0x10FFFF){
        bytes=4;
    }
    switch(bytes){
        case 1:
            fputc(character, out);
            num+=1;
            break;
        case 2:
            fputc((character>>6)^0xC0, out);
            fputc((character&0x3F)^0x80, out);
            num+=2;
            break;
        case 3:
            fputc((character>>12)^0xE0, out);
            fputc(((character>>6)&0x3F)^0x80, out);
            fputc((character&0x3F)^0x80, out);
            num+=3;
            break;
        case 4:
            fputc((character>>18)^0xF0, out);
            fputc(((character>>12)&0x3F)^0x80, out);
            fputc(((character>>6)&0x3F)^0x80, out);
            fputc((character&0x3F)^0x80, out);
            num+=4;
            break;
        default:
            return -1;
    }
    return num;
}

int traverseRules(FILE *out, int num){
    SYMBOL *currentRule=main_rule;
    SYMBOL *currentSymbol=currentRule;
    while(1){
        int character=currentSymbol->value;
        num=convert_utf8(character, out, num);
        if(num==-1){
            return -1;
        }
        if(currentSymbol->next==currentRule){
            if(currentRule->nextr==main_rule){
                num++;
                fputc(0x84, out);
                break;
            }
            else{
                num++;
                fputc(0x85, out);
                currentRule=currentRule->nextr;
                currentSymbol=currentRule;
            }
        }
        else{
            currentSymbol=currentSymbol->next;
        }
    }
    return num;
}
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
    debug("bSize is %d----------------------------------------------------------", bsize);
    int character;
    int num=0;
    init_symbols();
    init_rules();
    init_digram_hash();
    fputc(0x81, out);
    num++;
    int currentSize=0;
    while((character=fgetc(in))!=EOF){
        if(currentSize==0){
            fputc(0x83, out);
            num++;
            init_symbols();
            init_rules();
            init_digram_hash();
            debug("next nonterminalvalue: %d-------------------------------------------------", next_nonterminal_value);
            add_rule(new_rule(next_nonterminal_value));
            next_nonterminal_value++;
        }
        else if(currentSize==bsize){
            debug("one block ends----------------------------------------------------------------");
            num=traverseRules(out, num);
            if(num==-1){
                return EOF;
            }
            currentSize=0;
            continue;
        }
        SYMBOL *last=new_symbol(character, NULL);
        insert_after(main_rule->prev, last);
        check_digram(last->prev);
        currentSize++;
    }
    if(currentSize!=0){
        num=traverseRules(out,num);
        if(num==-1){
            return EOF;
        }
    }
    fputc(0x82, out);
    fflush(out);
    num++;
    debug("compression num is : %d", num);
    // To be implemented.
    return num;
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
    return (b1<<(6*(num-1)))+blater;
}
void add_symbol(SYMBOL *symbol, SYMBOL *currentRule){
    SYMBOL *temp;
    temp=currentRule->prev;
    currentRule->prev=symbol;
    temp->next=symbol;
    symbol->prev=temp;
    symbol->next=currentRule;
}

int expands(FILE *out, SYMBOL *currentRule, int num){
    SYMBOL *currentSymbol=currentRule->next;
    while(currentSymbol!=currentRule){
        if(IS_NONTERMINAL(currentSymbol)){
            if(currentSymbol->rule==NULL){
                currentSymbol->rule=*(rule_map+currentSymbol->value);
                if(currentSymbol->rule==currentRule||currentSymbol->rule==NULL){
                    return -1;
                }
            }
            num=expands(out, currentSymbol->rule, num);
        }
        else{
            fputc(currentSymbol->value, out);
            num++;
        }
        currentSymbol=currentSymbol->next;
    }
    return num;
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
    init_symbols();
    init_rules();
    int sot=0, blocks=0, eot=0;
    int character;
    int num=0;
    int rulePosition=0;
    SYMBOL *currentRule;
    while((character=fgetc(in))!=EOF){
        if(eot!=0){
            return EOF;
        }
        if(character==0x81){//start of transmission
            if(sot!=0){
                return EOF;
            }
            sot+=1;
            continue;
        }
        else if(sot==0){//if first byte is not SOT, error
            return EOF;
        }
        else if(character==0x83){//start of block
            if(blocks!=0){
                return EOF;
            }
            blocks+=1;
        }
        else if(character==0x82){//end of transmission
            eot+=1;
            fflush(out);
            break;
        }
        else if(character==0x84){//end of block
            if(blocks!=1||rulePosition==0){
                return EOF;
            }
            blocks-=1;
            num=expands(out, main_rule, num);
            init_symbols();
            init_rules();
            rulePosition=0;
        }
        else if(character==0x85){// end of a rule
            if(blocks!=1){
                return EOF;
            }
            if(currentRule==main_rule){
                if(rulePosition<2){
                    return EOF;
                }
            }
            else{
                if(rulePosition<3){
                    return EOF;
                }
            }
            rulePosition=0;
        }
        //wait for check if marker needs to be checked
        else{
            if(blocks!=1){
                return EOF;
            }
            character=convert(in, character);
            if(character==-1){
                return EOF;
            }
            else{
                if(rulePosition==0){
                    if(character<FIRST_NONTERMINAL||*(rule_map+character)!=NULL){
                        return EOF;
                    }
                    else{
                        currentRule=new_rule(character);
                        add_rule(currentRule);
                    }
                }
                else{
                    add_symbol(new_symbol(character, NULL), currentRule);
                }
                rulePosition++;
            }
        }
    }
    debug("decompression num is : %d", num);
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
                return 0;
            }
            else if(*temp=='c'){
                if(argc==2){
                    global_options=1024;
                    global_options=(global_options<<16)+2;
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
                            return 0;
                        }
                    }
                }
            }
            else if(*temp=='d'){
                if(argc==2){
                    global_options=4;
                    return 0;
                }
            }
        }
    }
    return -1;
}
