#include <stdlib.h>
#include <unistd.h>

#include "m_pd.h"  
#include "string.h"
#include "s7.h"
#include "time.h"
 
#define MAX_OUTLETS 32
#define MAX_ATOMS_PER_MESSAGE 1024
#define MAX_ATOMS_PER_OUTPUT_LIST 1024

static t_class *s4pd_class;  

// struct used (as void pointer) for clock scheduling  
typedef struct _s4pd_clock_info {
   //t_s4pd *obj;
   void *obj;
   t_symbol *handle; 
   t_clock *clock;
   struct _s4pd_clock_info *previous;
   struct _s4pd_clock_info *next;
} t_s4pd_clock_info;

typedef struct _s4pd {  
    t_object x_obj;
    s7_scheme *s7;         // pointer to the s7 instance
    bool  log_repl;        // whether to automatically post return values from S7 interpreter to console
    bool  log_null;        // whether to log return values that are null, unspecified, or gensyms
    int   num_outlets;
    t_outlet *outlets[MAX_OUTLETS];
    t_symbol *filename;

    t_s4pd_clock_info *first_clock;   // DUL of clocks 
    t_s4pd_clock_info *last_clock;    // keep pointer to most recent clock
} t_s4pd;  

void s4pd_free(t_s4pd *x);
void s4pd_load_from_path(t_s4pd *x, char *filename);
void s4pd_s7_load(t_s4pd *x, char *full_path);
void s4pd_post_s7_res(t_s4pd *x, s7_pointer res);
void s4pd_s7_eval_string(t_s4pd *x, char *string_to_eval);
void s4pd_s7_call(t_s4pd *x, s7_pointer funct, s7_pointer args);
void s4pd_reset(t_s4pd *x);
void s4pd_log_null(t_s4pd *x, t_floatarg f);
void s4pd_log_repl(t_s4pd *x, t_floatarg f);
void s4pd_message(t_s4pd *x, t_symbol *s, int argc, t_atom *argv);

static s7_pointer s7_load_from_path(s7_scheme *s7, s7_pointer args);
static s7_pointer s7_pd_output(s7_scheme *s7, s7_pointer args);
static s7_pointer s7_post(s7_scheme *s7, s7_pointer args);
static s7_pointer s7_send(s7_scheme *s7, s7_pointer args);
static s7_pointer s7_table_read(s7_scheme *s7, s7_pointer args);
static s7_pointer s7_table_write(s7_scheme *s7, s7_pointer args);
static s7_pointer s7_schedule_delay(s7_scheme *s7, s7_pointer args);
static s7_pointer s7_cancel_delay(s7_scheme *s7, s7_pointer args);
static s7_pointer s7_cancel_clocks(s7_scheme *s7, s7_pointer args);

int s7_obj_to_atom(s7_scheme *s7, s7_pointer *s7_obj, t_atom *atom);

void s4pd_clock_callback(void *arg);
void s4pd_remove_clock(t_s4pd *x, t_s4pd_clock_info *clock_info);
void s4pd_cancel_clocks(t_s4pd *x);


/********************************************************************************/
// some helpers for string/symbol handling 
// return true if a string begins and ends with quotes
int in_quotes(char *string){
    //post("in_quotes, input: %s", string);
    if(string[0] == '"' && string[ strlen(string)-1 ] == '"'){
        return 1;
    }else{
        return 0;
    }
} 
char *trim_quotes(char *input){
    int length = strlen(input);
    char *trimmed = malloc( sizeof(char*) * length );
    for(int i=0, j=0; i<length; i++){
        if( input[i] != '"'){ trimmed[j] = input[i]; j++; }    
    }
    return trimmed;
}   
// return true if a string starts with a single quote
int is_quoted_symbol(char *string){
    if(string[0] == '\'' && string[ strlen(string)-1 ] != '\''){
        return 1;
    }else{
        return 0;
    }
}
char *trim_symbol_quote(char *input){
    // drop the first character (the quote)
    char *trimmed = malloc( sizeof(char*) * (strlen(input) - 1) );
    int i;
    for(i=1; input[i] != '\0'; i++){
        trimmed[i-1] = input[i]; 
    }
    trimmed[i-1] = '\0';
    return trimmed;
}

// convert a Pd atom to the appropriate type of s7 pointer
// only handles basic types for now but is working
s7_pointer atom_to_s7_obj(s7_scheme *s7, t_atom *ap){
    // post("atom_to_s7_obj()");
    s7_pointer s7_obj;

    switch(ap->a_type){
        // Pd floats get turned into s7 reals. not sure how to deal with lack of ints in Pd!
        case A_FLOAT: 
            // post(" ... creating s7 real");
            s7_obj = s7_make_real(s7, atom_getfloat(ap));
            break;
        // Pd symbols could be any of: strings, symbols, quoted symbols, symbols for #t or #f
        case A_SYMBOL: 
            // post("A_SYMBOL %ld: %s", atom_getsymbol(ap)->s_name);
            // if sent \"foobar\" from max, we want an S7 string "foobar"
            if( in_quotes(atom_getsymbol(ap)->s_name) ){
                char *trimmed_sym = trim_quotes(atom_getsymbol(ap)->s_name);
                // post(" ... creating s7 string");
                s7_obj = s7_make_string(s7, trimmed_sym);
            }else if( is_quoted_symbol(atom_getsymbol(ap)->s_name) ){
            // if sent 'foobar, we actually want in s7 the list: ('quote 'foobar)
                // post(" ... creating s7 quoted symbol");
                s7_obj = s7_nil(s7); 
                s7_obj = s7_cons(s7, s7_make_symbol(s7, trim_symbol_quote(atom_getsymbol(ap)->s_name)), s7_obj);
                s7_obj = s7_cons(s7, s7_make_symbol(s7, "quote"), s7_obj); 
            }else{
                // otherwise, make it an s7 symbol
                // NB: foo -> foo, 'foo -> (symbol "foo")
                t_symbol *sym = atom_getsymbol(ap); 
                // post(" ... creating s7 symbol from %s ", sym->s_name);
                if( sym == gensym("#t") || sym == gensym("#true") ){
                    s7_obj = s7_t(s7);
                }else if( sym == gensym("#f") || sym == gensym("#false") ){
                    s7_obj = s7_f(s7);
                }else{
                    s7_obj = s7_make_symbol(s7, sym->s_name);
                }
            }
            break;
         default:
            // unhandled types return an s7 nil
            post("s4pd: unhandled atom type (%ld)", ap->a_type);
            s7_obj = s7_nil(s7);
    }
    // post("returning s7 obj: %s", s7_object_to_c_string(s7, s7_obj));
    return s7_obj;
}

int s7_obj_to_atom(s7_scheme *s7, s7_pointer *s7_obj, t_atom *atom){
    //post("s7_obj_to_atom");

    // TODO: not yet handling lists or vectors (in Max, these become atom arrays)
    // TODO: not yet handling hashtables (in Max, these become dicts)
    
    // booleans are cast to floats 
    if( s7_is_boolean(s7_obj) ){
        //post("creating Pd 1 or 0 float from s7 boolean");
        SETFLOAT(atom, (float)s7_boolean(s7, s7_obj));  
    }
    else if( s7_is_integer(s7_obj)){
        //post("creating int atom, %i", s7_integer(s7_obj));
        SETFLOAT(atom, s7_integer(s7_obj));
    }
    else if( s7_is_real(s7_obj)){
        //post("creating float atom, %.2f", s7_real(s7_obj));
        SETFLOAT(atom, s7_real(s7_obj));
    }
    else if( s7_is_symbol(s7_obj) ){
        // both s7 symbols and strings are converted to symbols
        //post("creating symbol atom, %s", s7_symbol_name(s7_obj));
        SETSYMBOL(atom, gensym( s7_symbol_name(s7_obj)));
    }
    else if( s7_is_string(s7_obj) ){
        //post("creating symbol atom from string, %s", s7_string(s7_obj));
        SETSYMBOL(atom, gensym( s7_string(s7_obj)));
    }
    else if( s7_is_character(s7_obj) ){
        //post("creating symbol atom from character");
        char out[2] = " \0";
        out[0] = s7_character(s7_obj);
        SETSYMBOL(atom, gensym(out));
    }
    else{
        post("ERROR: unhandled Scheme to Max conversion for: %s", s7_object_to_c_string(s7, s7_obj));
        return 1;     
    } 
    return 0;
}

// get a pd struct pointer from the s7 environment pointer
t_s4pd *get_pd_obj(s7_scheme *s7){
    // get our max object by reading the max pointer from the scheme environment
    uintptr_t s4pd_ptr_from_s7 = (uintptr_t)s7_integer( s7_name_to_value(s7, "pd-obj") );
    t_s4pd *s4pd_ptr = (t_s4pd *)s4pd_ptr_from_s7;
    return s4pd_ptr;
}

static s7_pointer s7_load_from_path(s7_scheme *s7, s7_pointer args){
    t_s4pd *x = get_pd_obj(s7);
    // get filename from s7
    char *filename = s7_string( s7_car(args) );
    //post("s7_load_from_path %s", filename);
    // use open_via_path to get full path, but then don't load from the descriptor
    int filedesc;
    char buf[MAXPDSTRING], *bufptr;
    char load_string[MAXPDSTRING];
    if((filedesc = open_via_path("", filename, "", buf, &bufptr, MAXPDSTRING, 0)) < 0){
        pd_error("%s: can't open", filename);
        return s7_nil(s7);
    }
    close(filedesc);
    sprintf(load_string, "(load \"%s/%s\")", buf, filename);
    s4pd_s7_eval_string(x, load_string);
    return s7_nil(s7);    
}

// function to send generic output out an outlet
static s7_pointer s7_post(s7_scheme *s7, s7_pointer args){
    //post("s7_post, args: %s", s7_object_to_c_string(s7, args));
    char *str_repr = s7_object_to_c_string(s7, args) + 1;
    str_repr[ strlen(str_repr) - 1] = '\0';
    // don't print the opening and closing parens
    post("s4pd: %s", str_repr);
    return s7_nil(s7); 
}

// function to send a message to a receiver
static s7_pointer s7_send(s7_scheme *s7, s7_pointer args){
    // post("s7_send, args: %s", s7_object_to_c_string(s7, args));
    t_s4pd *x = get_pd_obj(s7);
    // first arg must be a symbol for the receiver
    int err;
    char *receiver_name;
    char *msg_symbol;
    // where we look in s7 args for max method args, normally 2
    int starting_arg = 2;   
    // initialize return value to nil, as we need to return something to S7 even on errors
    s7_pointer *s7_return_value = s7_nil(x->s7); 
  
    // get symbol from first s7 arg
    if( s7_is_symbol( s7_car(args) ) ){ 
        receiver_name = s7_symbol_name( s7_car(args) );
    }else{
        pd_error((t_object *)x, "s4pd: (send): arg 1 must be a symbol of a receiver name");
        return s7_return_value;
    }
    if( ! gensym(receiver_name)->s_thing ){
        pd_error((t_object *)x, "s4pd: (send): no receiver found");
        return s7_return_value;
    }
   
    // message to be sent could be an int, real, or symbol
    // NB: in PD & Max, a message "1 2 3" is actually sent internally as "list 1 2 3"
    if( s7_is_symbol( s7_cadr(args) ) ){ 
        msg_symbol = s7_symbol_name( s7_cadr(args) );
    }else if( s7_is_number( s7_cadr(args) ) ){
        // if the first arg is a number, figure out if message is one number or a list
        if( s7_list_length(s7, args) <= 2 ){
            msg_symbol = "float";
        }else{
            msg_symbol = "list"; 
        }
        starting_arg = 1;
    }else{
        pd_error((t_object *)x, "s4pd: (send): arg 2 should be a symbol of the message to send");
        return s7_return_value;
    }
    // loop through the args to build an atom list of the right length
    // TODO learn how to do this correctly, and add error handling for over the limit yo
    t_atom arg_atoms[ MAX_ATOMS_PER_MESSAGE ];
    int num_atoms = s7_list_length(s7, args) - starting_arg;
    // build arg list of atoms
    for(int i=0; i < num_atoms; i++){
        err = s7_obj_to_atom(s7, s7_list_ref(s7, args, i + starting_arg), arg_atoms + i );     
        if(err){
            pd_error((t_object *)x, "s4pd: (send): error converting Scheme arg to Pd atom, aborting");
            return s7_return_value;
        }
    }
    // now we can send the message to the receiver
    //     err = object_method_typed(obj, gensym(msg_symbol), num_atoms, arg_atoms, NULL);
    typedmess( gensym(receiver_name)->s_thing, gensym(msg_symbol), num_atoms, arg_atoms);
    return s7_return_value;
}


// read a value from an Pd array
static s7_pointer s7_table_read(s7_scheme *s7, s7_pointer args){
    //post("s7_table_read, args: %s", s7_object_to_c_string(s7, args));
    t_s4pd *x = get_pd_obj(s7);
    // initialize return value to false, which means could not get value
    s7_pointer *s7_return_value = s7_make_boolean(x->s7, false); 

    char *array_name;
    t_int  arr_index = 0;
    t_garray *array;
    t_int num_points;
    t_word *values_vector;

    // get and check array name and index point from s7_args
    if( s7_is_symbol( s7_car(args) ) ){ 
        array_name = s7_symbol_name( s7_car(args) );
    }else{
        pd_error((t_object *)x, "s4pd: (table-read): arg 1 must be a symbol of an array name");
        return s7_return_value;
    }
    if( s7_is_number(s7_cadr(args))){
        arr_index = s7_integer(s7_cadr(args)); 
    }else{
        pd_error((t_object *)x, "s4pd: (table-read): arg 2 must be an index");
        return s7_return_value;
    }
    // get the array contents (altered from Pd's d_array.c)
    if (!(array = (t_garray *)pd_findbyclass( gensym(array_name), garray_class))){
        pd_error(x, "s4pd: no array named %s", array_name);
        return s7_return_value;
    }else if (!garray_getfloatwords(array, &num_points, &values_vector)){
        pd_error(x, "s4pd: error in table-read reading %s", array_name);
        return s7_return_value;
    }
    // check index in range
    if( arr_index < 0 || arr_index >= num_points ){
        pd_error((t_object *)x, "s4pd: (table-read): index out of range");
        return s7_return_value;
    }
    // get our float and return to s7 
    s7_return_value = s7_make_real(x->s7, values_vector[ arr_index ].w_float);  
    return s7_return_value;
}

// write a float to a Pd array
static s7_pointer s7_table_write(s7_scheme *s7, s7_pointer args){
    //post("s7_table_write, args: %s", s7_object_to_c_string(s7, args));
    t_s4pd *x = get_pd_obj(s7);
    // initialize return value to false, which means error writing
    // returns value written on success
    s7_pointer *s7_return_value = s7_make_boolean(x->s7, false); 

    char *array_name;
    t_int index;
    t_float value;

    t_garray *array;
    t_int num_points;
    t_word *values_vector;

    // get and check array name, index point, and value from s7_args
    if( s7_is_symbol( s7_car(args) ) ){ 
        array_name = s7_symbol_name( s7_car(args) );
    }else{
        pd_error((t_object *)x, "s4pd: (table-write): arg 1 must be a symbol of an array name");
        return s7_return_value;
    }
    if( s7_is_number(s7_cadr(args))){
        index = s7_integer(s7_cadr(args)); 
    }else{
        pd_error((t_object *)x, "s4pd: (table-write): arg 2 must be an index");
        return s7_return_value;
    }
    if( s7_is_number(s7_caddr(args))){
        value = s7_real(s7_caddr(args)); 
    }else{
        pd_error((t_object *)x, "s4pd: (table-write): arg 3 must be a number");
        return s7_return_value;
    }
    //post("tabw %s %i %5.2f", array_name, index, value);

    // get the array contents (altered from Pd's d_array.c)
    if (!(array = (t_garray *)pd_findbyclass( gensym(array_name), garray_class))){
        pd_error(x, "s4pd: (table-write) no array named %s", array_name);
        return s7_return_value;
    }else if (!garray_getfloatwords(array, &num_points, &values_vector)){
        pd_error(x, "s4pd: (table-write) error reading %s", array_name);
        return s7_return_value;
    }
    // check index in range
    if( index < 0 || index >= num_points ){
        pd_error((t_object *)x, "s4pd: (table-write): index out of range");
        return s7_return_value;
    }
    // update array, call redraw, and return written value
    values_vector[ index ].w_float = value;
    garray_redraw(array);
    s7_return_value = s7_make_real(x->s7, value);
    return s7_return_value;
}

// send output out an outlet
static s7_pointer s7_pd_output(s7_scheme *s7, s7_pointer args){
    //post("s7_pd_output, args: %s", s7_object_to_c_string(s7, args));
    
    // all added functions have this form, args is a list, s7_car(args) is the first arg, etc 
    int outlet_num = (int) s7_real( s7_car(args) );
    t_s4pd *x = get_pd_obj(s7);

    // check if outlet number exists
    if( outlet_num > x->num_outlets || outlet_num < 0 ){
        pd_error((t_object *)x, "ERROR: invalid outlet number %i", outlet_num);
        return s7_nil(s7);
    }
    s7_pointer s7_out_val = s7_cadr(args);
    t_symbol *msg_sym;  // the first symbol for the outlet_anything message 
    t_atom output_atom; 
    int err;
    //post("  - s7_out_val: %s", s7_object_to_c_string(s7, s7_out_val));

    // bools and all numbers become pd floats 
    if( s7_is_real(s7_out_val) || s7_is_boolean(s7_out_val) ){
        err = s7_obj_to_atom(s7, s7_out_val, &output_atom);
        if( err ){
          post("s4pd: error outputing %s", s7_object_to_c_string(s7, s7_out_val));
        }else{
          outlet_anything( x->outlets[outlet_num], gensym("float"), 1, &output_atom);
        }
    }
    // symbols, keywords, chars, and strings all become pd symbols
    // in PD, symbol messages always start with 'symbol', unlike Max, where the message type IS the symbol
    else if( s7_is_string(s7_out_val) || s7_is_symbol(s7_out_val) || s7_is_character(s7_out_val) ){
        //post(" - output is a symbol");
        // note that symbol catches keywords as well
        err = s7_obj_to_atom(s7, s7_out_val, &output_atom);
        if( err ){
            post("s4pd: error outputing %s", s7_object_to_c_string(s7, s7_out_val));
        }else{
            // note that unlike Max, the output message is always "symbol {thing}"
            outlet_anything(x->outlets[outlet_num], gensym("symbol"), 1, &output_atom); 
        }
    }
    // lists
    else if( s7_is_list(s7, s7_out_val) && !s7_is_null(s7, s7_out_val) ){
        // array of atoms to output, we overallocate for now rather than do dynamic allocation 
        t_atom out_list[MAX_ATOMS_PER_OUTPUT_LIST];
        s7_pointer *first = s7_car(s7_out_val);
        int length = s7_list_length(s7, s7_out_val);

        // lists have have two cases: start with symbol or start with number/bool
        if( s7_is_number(first) || s7_is_boolean(first) ){
            //post("outputting list with numeric or bool first arg, becomes 'list' message");
            for(int i=0; i<length; i++){
                s7_obj_to_atom(s7, s7_list_ref(s7, s7_out_val, i), &out_list[i]);
            }
            // add the symbol "list" as the first item for the message output
            outlet_anything( x->outlets[outlet_num], gensym("list"), length, out_list);     
        }
        else {
            //post("list starting with a symbol");     
            // build the atom list, starting from the second item 
            for(int i=1; i<length; i++){
                s7_obj_to_atom(s7, s7_list_ref(s7, s7_out_val, i), &out_list[i - 1]);
            }
            // use the first item of the list as the symbolic message selector
            // thus to output a 'list' message, user can do (out 0 '(list :a :b :c))
            err = s7_obj_to_atom(s7, first, &output_atom); 
            outlet_anything( x->outlets[outlet_num], atom_getsymbol(&output_atom), length - 1, out_list);     
        }
    }
    // vectors are supported for bool, int, float only
    // TODO: should support output of vectors of symbols too I think... 
    else if( s7_is_vector(s7_out_val) && s7_vector_length(s7_out_val) > 0 ){
        t_atom out_list[MAX_ATOMS_PER_OUTPUT_LIST];
        int length = s7_vector_length(s7_out_val);
        for(int i=0; i<length; i++){
            // if invalid type, return with error
            s7_pointer *item = s7_vector_ref(s7, s7_out_val, i);
            if( s7_is_number(item) || s7_is_boolean(item)){
                s7_obj_to_atom(s7, item, &out_list[i]);
            }else{
                error("s4pd: Vector output only supported for ints, floats, & booleans");
                return s7_nil(s7);
            }
        }
        // didn't hit an invalid type, we can output the list
        outlet_anything( x->outlets[outlet_num], gensym("list"), length, out_list);     
    } 
    // unhandled output type, post an error
    else{
        error("s4pd: Unhandled output type %s", s7_object_to_c_string(s7, s7_out_val));
    }
    // returns nil so that the console is not chatting on every output message
    return s7_nil(s7);
}

void s4pd_message(t_s4pd *x, t_symbol *s, int argc, t_atom *argv){
    //post("s4pd_message() *s: '%s' argc: %i", s->s_name, argc);

    // case for code as a symbol
    if( s == gensym("symbol")){
        //post("hanlding scheme code in a symbol message");
        t_symbol *code_sym = atom_getsymbol(argv); 
        s4pd_s7_eval_string(x, code_sym->s_name);
    }
    // case for code as generic list of atoms
    else{
        t_atom *ap;
        s7_pointer s7_args = s7_nil(x->s7); 
        // loop through the args backwards to build the cons list 
        for(int i = argc-1; i >= 0; i--) {
            ap = argv + i;
            s7_args = s7_cons(x->s7, atom_to_s7_obj(x->s7, ap), s7_args); 
        }
        // add the first message to the arg list (it's always a symbol)
        // call the s7 eval function, sending in all args as an s7 list
        s7_args = s7_cons(x->s7, s7_make_symbol(x->s7, s->s_name), s7_args); 
        //post("  - s7_args: %s", s7_object_to_c_string(x->s7, s7_args));
        s4pd_s7_call(x, s7_name_to_value(x->s7, "s4pd-eval"), s7_args);
    }
}

void s4pd_init_s7(t_s4pd *x){
    //post("s4pd_init_s7()");
    // start the S7 interpreter 
    x->s7 = s7_init();
    
    s7_define_function(x->s7, "load-from-path", s7_load_from_path, 1, 0, false, "load a file using the search path");
    s7_define_function(x->s7, "out", s7_pd_output, 2, 0, false, "(out 1 99) sends value 99 out outlet 1");
    s7_define_function(x->s7, "post", s7_post, 1, 0, true, "posts output to the console");
    s7_define_function(x->s7, "send", s7_send, 2, 0, true, "sends message to a receiver");

    s7_define_function(x->s7, "table-read", s7_table_read, 2, 0, false, "read a point from an array");
    s7_define_function(x->s7, "tabr", s7_table_read, 2, 0, false, "read a point from an array");
    s7_define_function(x->s7, "table-write", s7_table_write, 3, 0, false, "write a point to an array");
    s7_define_function(x->s7, "tabw", s7_table_write, 3, 0, false, "write a point to an array");

    s7_define_function(x->s7, "s4pd-schedule-delay", s7_schedule_delay, 2, 0, false, "schedule a delay callback");
    s7_define_function(x->s7, "s4pd-cancel-clocks", s7_cancel_clocks, 0, 0, false, "cancel all clocks");
    // not in use right now, might bring it back later
    //s7_define_function(x->s7, "s4pd-cancel-delay", s7_cancel_delay, 1, 0, false, "cancel and free a clock delay");

    // make the address of this object available in scheme as "pd-obj" so that 
    // scheme functions can get access to our C functions
    uintptr_t pd_obj_ptr = (uintptr_t)x;
    s7_define_variable(x->s7, "pd-obj", s7_make_integer(x->s7, pd_obj_ptr));  

    s7_pointer load_flags = s7_eval_c_string(x->s7, "(begin (define s4pd-loaded #f) (define s4pd-schedule-loaded #f))");

    // load the bootstrap file
    // TODO: should it look for an s4pd in the working dir first??
    s4pd_load_from_path(x, "s4pd.scm");

    // if file arg used, load it
    if( x->filename != gensym("") ){
      //post("loading file arg: %s", x->filename->s_name);
      s4pd_load_from_path(x, x->filename->s_name);
    }

    // check if the bootfiles loaded ok
    s7_pointer loaded_ok = s7_eval_c_string(x->s7, "(and s4pd-loaded s4pd-schedule-loaded)");
    if( !s7_boolean(x->s7, loaded_ok) ){
        pd_error((t_object *)x, 
"ERROR: s4pd.scm and s4pd-schedule.scm did not load.\n\
Check that sp4d/scm dir is on your Pd file path.\n\
The interpreter will run but the s74 additions and the delay function will not be working.");
    }else{
      post("s4pd initialized");
    }
}


void *s4pd_new(t_symbol *s, int argc, t_atom *argv){  
    //post("s4pd_new(), argc: %i", argc);
    t_s4pd *x = (t_s4pd *) pd_new (s4pd_class);

    // set up default vars
    x->log_repl = false;
    x->log_null = false;
    x->num_outlets = 1;
    x->filename = gensym("");
    
    // init the clock info pointer double linked list
    x->first_clock = x->last_clock = NULL;

    // if args are given, they are: outlets, filename
    switch(argc){
      default:
      case 2:
        x->filename = atom_getsymbol(argv+1);
      case 1:
        x->num_outlets = atom_getint(argv);
      case 0:
        break;
    }
    //post("s4pd_new() outlets: %i filename: %s", x->num_outlets, x->filename->s_name);
    // make the outlets
    for(int i = 0; i < x->num_outlets; i++){
      x->outlets[i] = outlet_new(&x->x_obj, 0);
    }

    // set up the s7 interpreter
    s4pd_init_s7(x);
       //post("... s4pd_new() done");
    return (void *)x;  
}  
 
void s4pd_setup(void) {  
    s4pd_class = class_new(
        gensym("s4pd"),  
        (t_newmethod)s4pd_new, 
        (t_method)s4pd_free, 
        sizeof(t_s4pd), 
        CLASS_DEFAULT, 
        A_GIMME,        // allow dynamic number of arguments
        0);  
    class_addmethod(s4pd_class, (t_method)s4pd_log_null, gensym("log-null"), A_DEFFLOAT, 0);
    class_addmethod(s4pd_class, (t_method)s4pd_log_repl, gensym("log-repl"), A_DEFFLOAT, 0);
    class_addmethod(s4pd_class, (t_method)s4pd_reset, gensym("reset"), 0);
    class_addmethod(s4pd_class, (t_method)s4pd_cancel_clocks, gensym("cancel-clocks"), 0);
    class_addanything(s4pd_class, (t_method)s4pd_message);
}

void s4pd_free(t_s4pd *x){
    s4pd_cancel_clocks(x);
    s4pd_init_s7(x);
}

void s4pd_reset(t_s4pd *x){
    //post("s4pd_reset()");
    // cancel and free any outstanding delays by calling clear-delays in scheme
    s4pd_cancel_clocks(x);
    s4pd_init_s7(x); 
    post("s7 reinitialized"); 
}

void s4pd_log_null(t_s4pd *x, t_floatarg arg){
    //post("log_null()"); 
    x->log_null = (int) arg == 0 ? false : true;
    //post(" x.log_null now: %i", x->log_null);
}

void s4pd_log_repl(t_s4pd *x, t_floatarg arg){
    //post("log_repl()"); 
    x->log_repl = (int) arg == 0 ? false : true;
    //post(" x.log_repl now: %i", x->log_repl);
}

void s4pd_post_s7_res(t_s4pd *x, s7_pointer res) {
    // TODO add gensym filter
    char *log_out = s7_object_to_c_string(x->s7, res);
    if(s7_is_null(x->s7, res) || s7_is_unspecified(x->s7, res)
      || strstr(log_out, "{gensym}-")){ 
      if( !x->log_repl || !x->log_null) return;
    }else{
      if(!x->log_repl) return;
    }
    post("s4pd> %s", log_out);
}


// eval string  with error logging
void s4pd_s7_eval_string(t_s4pd *x, char *string_to_eval){
    //post("s4pd_s7_eval_string() %s", string_to_eval);
    int gc_loc;
    s7_pointer old_port;
    const char *errmsg = NULL;
    char *msg = NULL;
    old_port = s7_set_current_error_port(x->s7, s7_open_output_string(x->s7));
    gc_loc = s7_gc_protect(x->s7, old_port);
    //post("calling s7_eval_c_string");
    s7_pointer res = s7_eval_c_string(x->s7, string_to_eval);
    errmsg = s7_get_output_string(x->s7, s7_current_error_port(x->s7));
    if ((errmsg) && (*errmsg)){
        msg = (char *)calloc(strlen(errmsg) + 1, sizeof(char));
        strcpy(msg, errmsg);
    }
    s7_close_output_port(x->s7, s7_current_error_port(x->s7));
    s7_set_current_error_port(x->s7, old_port);
    s7_gc_unprotect_at(x->s7, gc_loc);
    if (msg){
        //object_error((t_object *)x, "s4pd Error: %s", msg);
        post("s4pd Error: %s", msg);
        free(msg);
    }else{
        if(x->log_repl) s4pd_post_s7_res(x, res);
    }
}

// call s7_call, with error logging
void s4pd_s7_call(t_s4pd *x, s7_pointer funct, s7_pointer args){
    //post("s4pd_s7_call()");
    //post(" - function: %s args: %s", s7_object_to_c_string(x->s7, funct), s7_object_to_c_string(x->s7, args));
    int gc_loc;
    s7_pointer old_port;
    const char *errmsg = NULL;
    char *msg = NULL;
    old_port = s7_set_current_error_port(x->s7, s7_open_output_string(x->s7));
    gc_loc = s7_gc_protect(x->s7, old_port);
    // the actual call
    s7_pointer res = s7_call(x->s7, funct, args);
    errmsg = s7_get_output_string(x->s7, s7_current_error_port(x->s7));
    if ((errmsg) && (*errmsg)){
        msg = (char *)calloc(strlen(errmsg) + 1, sizeof(char));
        strcpy(msg, errmsg);
    }
    s7_close_output_port(x->s7, s7_current_error_port(x->s7));
    s7_set_current_error_port(x->s7, old_port);
    s7_gc_unprotect_at(x->s7, gc_loc);
    if (msg){
        post("s4pd Error: %s", msg);
        free(msg);
    }else{
        //post(" res from call: %s", s7_object_to_c_string(x->s7, res));
        if(x->log_repl) s4pd_post_s7_res(x, res);
    }
}

// call s7_load using the pd searchpath
void s4pd_load_from_path(t_s4pd *x, char *filename){
    //post("s4pd_load_from_path() %s", filename);
    // use open_via_path to get full path, but then don't load from the descriptor
    int filedesc;
    char buf[MAXPDSTRING], *bufptr;
    char full_path[MAXPDSTRING];
    if((filedesc = open_via_path("", filename, "", buf, &bufptr, MAXPDSTRING, 0)) < 0){
        error("%s: can't open", filename);
        return;    
    }
    close(filedesc);
    sprintf(full_path, "%s/%s", buf, filename);
    s4pd_s7_load(x, full_path);
}

// call s7_load using fullpath, with error logging
void s4pd_s7_load(t_s4pd *x, char *full_path){
    //post("s4pd_s7_load() %s", full_path);
    int gc_loc;
    s7_pointer old_port;
    const char *errmsg = NULL;
    char *msg = NULL;
    old_port = s7_set_current_error_port(x->s7, s7_open_output_string(x->s7));
    gc_loc = s7_gc_protect(x->s7, old_port);
    s7_load(x->s7, full_path);
    errmsg = s7_get_output_string(x->s7, s7_current_error_port(x->s7));
    if ((errmsg) && (*errmsg)){
        msg = (char *)calloc(strlen(errmsg) + 1, sizeof(char));
        strcpy(msg, errmsg);
    }
    s7_close_output_port(x->s7, s7_current_error_port(x->s7));
    s7_set_current_error_port(x->s7, old_port);
    s7_gc_unprotect_at(x->s7, gc_loc);
    if (msg){
        pd_error((t_object *)x, "s4pd Error loading %s: %s", full_path, msg);
        free(msg);
    }else{
        // we don't run this in production as the res printed is the last line of
        // the file loaded, which looks weird to the user
    }
}


/*********************************************************************************
* Scheduler and clock stuff */

// delay a function using Pd clock objects for floating point precision delays
// called from scheme as (s4pd-schedule-delay)
static s7_pointer s7_schedule_delay(s7_scheme *s7, s7_pointer args){
    //post("s7_schedule_delay()");
    t_s4pd *x = get_pd_obj(s7);
    char *cb_handle_str;
    // first arg is float of time in ms 
    double delay_time = s7_real( s7_car(args) );
    // second arg is the symbol from the gensym call in Scheme
    s7_pointer *s7_cb_handle = s7_cadr(args);
    cb_handle_str = s7_symbol_name(s7_cb_handle);
    //post("s7_schedule_delay() time: %5.2f handle: '%s'", delay_time, cb_handle_str);

    //  allocate memory for the clock_info struct, holds gensym handle, ref to s4pd, and ref to clock 
    // NB: this gets freed by clock callback after clock fires
    t_s4pd_clock_info *clock_info = (t_s4pd_clock_info *)getbytes(sizeof(t_s4pd_clock_info));
    clock_info->obj = (void *)x;
    clock_info->handle = gensym(cb_handle_str);
    // make a clock, setting our clock_info struct as the owner, as void pointer
    // when the callback method fires, it will retrieve this pointer as an arg 
    // and use it to get the handle for calling into scheme  
    t_clock *clock = clock_new( (void *)clock_info, (t_method)s4pd_clock_callback);
    // store the clock ref itself in there too
    clock_info->clock = clock;

    // put the new clock_info struct onto the clocks list at last place
    // TODO refactor the queue insertion into a store clock function later
    if(x->first_clock == NULL){ 
        //post(" adding clock to queue as first");
        // it's the only clock
        x->first_clock = clock_info;
        x->last_clock = clock_info;
        clock_info->previous = NULL;
        clock_info->next = NULL;
    }else{
        //post(" adding clock to queue as last");
        // else insert at end of list
        clock_info->previous = x->last_clock;
        x->last_clock->next = clock_info;
        clock_info->next = NULL;
        x->last_clock = clock_info;
    }

    // schedule the clock
    clock_delay(clock, delay_time);
    // return the handle on success so that scheme code can save it for possibly cancelling later
    return s7_make_symbol(s7, cb_handle_str);
}

// the callback that runs for any clock and is used to find the delayed function in Scheme
void s4pd_clock_callback(void *arg){
    // post("s4pd_clock_callback()");
    t_s4pd_clock_info *clock_info = (t_s4pd_clock_info *) arg;
    t_s4pd *x = (t_s4pd *)clock_info->obj;
    t_symbol *handle = clock_info->handle; 
    // post(" - handle %s", handle->s_name);
    // call into scheme with the handle, where scheme will call the registered delayed function
    s7_pointer *s7_args = s7_nil(x->s7);
    s7_args = s7_cons(x->s7, s7_make_symbol(x->s7, handle->s_name), s7_args); 
    s4pd_s7_call(x, s7_name_to_value(x->s7, "s4pd-execute-callback"), s7_args);   

    // clean up the clock stuff
    // detach it from the double linked list, could be first, last, or middle
    s4pd_remove_clock(x, clock_info); 
    // free the clock 
    clock_free(clock_info->clock);
    // and free the clock info struct
    freebytes(clock_info, sizeof( t_s4pd_clock_info ) );
}


// cancelling a single delay manually on C side
// NOT IN USE RIGHT NOW - delays just harmlessly fire off and Scheme does nothing
// might bring this back later with the new queue though
/*
static s7_pointer s7_cancel_delay(s7_scheme *s7, s7_pointer args){
    post("s7_cancel_delay");
    uintptr_t u_clock_info_ptr = (uintptr_t)s7_integer( s7_car(args) );
    t_s4pd_clock_info *clock_info_ptr = (t_s4pd_clock_info *) u_clock_info_ptr;    

    // free everything that was allocated for this delay instance

    // 2021-06-12: any of these (even individually) will make it crash
    // pd(99452,0x10cf1e5c0) malloc: *** error for object 0x106400110: pointer being freed was not allocated
    clock_unset(clock_info_ptr->clock); 
    clock_free(clock_info_ptr->clock);
    freebytes(clock_info_ptr, sizeof(clock_info_ptr));
}
*/  

// remove a clock_info pointer from the queue, updating queue head and tail
// this just extracts the clock, which could be anywhere in the queue
void s4pd_remove_clock(t_s4pd *x, t_s4pd_clock_info *clock_info){
    if( x->first_clock == clock_info && x->last_clock == clock_info){
        // case: only clock
        x->first_clock = x->last_clock = NULL;
    }else if( x->first_clock == clock_info ){
        // case: it's the first clock and has a next
        // new first clock pointer is this clock's next
        clock_info->next->previous = NULL;
        x->first_clock = clock_info->next;
    }else if( x->last_clock == clock_info && clock_info->previous){
        // case its the last clock, but has a previous
        clock_info->previous->next = NULL;
        x->last_clock = clock_info->previous;
    }else {
        // case, middle clock (no change to last or first pointer)
        clock_info->previous->next = clock_info->next;
        clock_info->next->previous = clock_info->previous;
    }
}

void s4pd_cancel_clocks(t_s4pd *x){
    //post("s4pd_cancel_clocks(): unsetting and freeing all clocks");
    t_s4pd_clock_info *clock_info; 
    while( clock_info = x->first_clock ){
        s4pd_remove_clock(x, clock_info );
        clock_unset( clock_info->clock );
        clock_free( clock_info->clock );
        freebytes(clock_info, sizeof(t_s4pd_clock_info));
    }
    //post(" - clocks removed");
}

// s7 method for cancelling clocks
static s7_pointer s7_cancel_clocks(s7_scheme *s7, s7_pointer args){
    //post("s7_cancel_clocks()");
    t_s4pd *x = get_pd_obj(s7);
    s4pd_cancel_clocks(x);
}
