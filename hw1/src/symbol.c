#include "const.h"
#include "sequitur.h"

/*
 * Symbol management.
 *
 * The functions here manage a statically allocated array of SYMBOL structures,
 * together with a stack of "recycled" symbols.
 */

/*
 * Initialization of this global variable that could not be performed in the header file.
 */
int next_nonterminal_value = FIRST_NONTERMINAL;
SYMBOL* recycled_symbols_list=NULL;
/**
 * Initialize the symbols module.
 * Frees all symbols, setting num_symbols to 0, and resets next_nonterminal_value
 * to FIRST_NONTERMINAL;
 */
void init_symbols(void) {
    for(int i=0; i<MAX_SYMBOLS; i++){
        SYMBOL *symbols=symbol_storage+i;
        symbols->value=0;
        symbols->rule=0;
        symbols->refcnt=0;
        symbols->next=0;
        symbols->prev=0;
        symbols->nextr=0;
        symbols->prevr=0;
    }
    num_symbols=0;
    next_nonterminal_value=FIRST_NONTERMINAL;
    recycled_symbols_list=NULL;
    // To be implemented.
}

/**
 * Get a new symbol.
 *
 * @param value  The value to be used for the symbol.  Whether the symbol is a terminal
 * symbol or a non-terminal symbol is determined by its value: terminal symbols have
 * "small" values (i.e. < FIRST_NONTERMINAL), and nonterminal symbols have "large" values
 * (i.e. >= FIRST_NONTERMINAL).
 * @param rule  For a terminal symbol, this parameter should be NULL.  For a nonterminal
 * symbol, this parameter can be used to specify a rule having that nonterminal at its head.
 * In that case, the reference count of the rule is increased by one and a pointer to the rule
 * is stored in the symbol.  This parameter can also be NULL for a nonterminal symbol if the
 * associated rule is not currently known and will be assigned later.
 * @return  A pointer to the new symbol, whose value and rule fields have been initialized
 * according to the parameters passed, and with other fields zeroed.  If the symbol storage
 * is exhausted and a new symbol cannot be created, then a message is printed to stderr and
 * abort() is called.
 *
 * When this function is called, if there are any recycled symbols, then one of those is removed
 * from the recycling list and used to satisfy the request.
 * Otherwise, if there currently are no recycled symbols, then a new symbol is allocated from
 * the main symbol_storage array and the num_symbols variable is incremented to record the
 * allocation.
 */
SYMBOL *new_symbol(int value, SYMBOL *rule) {
    if(num_symbols==MAX_SYMBOLS){
        fprintf(stderr, "%s\n", "Max number of symbols reached");
        abort();
    }
    SYMBOL *result;
    if(recycled_symbols_list==NULL){
        result=symbol_storage+num_symbols;
        num_symbols++;
    }
    else{
        result=recycled_symbols_list;
        recycled_symbols_list=recycled_symbols_list->prev;
        if(recycled_symbols_list){
            recycled_symbols_list->next=NULL;
        }
    }
    if(rule){
        ref_rule(rule);
    }
    result->value=value;
    result->rule=rule;
    result->refcnt=0;
    result->next=0;
    result->prev=0;
    result->nextr=0;
    result->prevr=0;
    debug("New %s symbol <%lu> (value=%d)", IS_TERMINAL(result)?"terminal":"nonterminal", SYMBOL_INDEX(result),result->value);
    return result;
    // To be implemented.
}

/**
 * Recycle a symbol that is no longer being used.
 *
 * @param s  The symbol to be recycled.  The caller must not use this symbol any more
 * once it has been recycled.
 *
 * Symbols being recycled are added to the recycled_symbols list, where they will
 * be made available for re-allocation by a subsequent call to new_symbol.
 * The recycled_symbols list is managed as a LIFO list (i.e. a stack), using the
 * next field of the SYMBOL structure to chain together the entries.
 */
void recycle_symbol(SYMBOL *s) {
    debug("recycle symbol <%lu>", SYMBOL_INDEX(s));
    if (recycled_symbols_list==NULL)
    {
        recycled_symbols_list=s;
        recycled_symbols_list->prev=NULL;
        recycled_symbols_list->next=NULL;
    }
    else{
        recycled_symbols_list->next=s;
        s->prev=recycled_symbols_list;
        s->next=NULL;
        recycled_symbols_list=s;
    }

    // To be implemented.
}
