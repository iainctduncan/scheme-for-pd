#include "m_pd.h"  
#include "string.h"
#include "s7.h"
 
#define MAX_OUTLETS 32
#define MAX_ATOMS_PER_MESSAGE 1024
#define MAX_ATOMS_PER_OUTPUT_LIST 1024

static t_class *s4pd_class;  
 
typedef struct _s4pd {  
    t_object x_obj;
    s7_scheme *s7;                  // pointer to the s7 instance
    bool  log_return_values;        // whether to automatically post return values from S7 interpreter to console
    bool  log_null;                 // whether to log return values that are null, unspecified, or gensyms
    int   num_outlets;
    t_outlet *outlets[MAX_OUTLETS];
    t_symbol *filename;
} t_s4pd;  
   
void s4pd_s7_load(t_s4pd *x, char *full_path);
void s4pd_s7_call(t_s4pd *x, s7_pointer funct, s7_pointer args);
void s4pd_s7_eval_string(t_s4pd *x, char *string_to_eval);
void s4pd_post_s7_res(t_s4pd *x, s7_pointer res);
void s4pd_s7_eval_string(t_s4pd *x, char *string_to_eval);
void s4pd_s7_call(t_s4pd *x, s7_pointer funct, s7_pointer args);

void s4pd_reset(t_s4pd *x);
void s4pd_log_null(t_s4pd *x, t_floatarg f);

void s4pd_message(t_s4pd *x, t_symbol *s, int argc, t_atom *argv);
static s7_pointer s7_pd_output(s7_scheme *s7, s7_pointer args);

// XXX: how are error codes handled in Pd??
int s7_obj_to_atom(s7_scheme *s7, s7_pointer *s7_obj, t_atom *atom);

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
            //post("A_SYMBOL %ld: %s", atom_getsymbol(ap)->s_name);
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
            post("ERROR: unknown atom type (%ld)", ap->a_type);
            s7_obj = s7_nil(s7);
    }
    // post("returning s7 obj: %s", s7_object_to_c_string(s7, s7_obj));
    return s7_obj;
}

int s7_obj_to_atom(s7_scheme *s7, s7_pointer *s7_obj, t_atom *atom){
    post("s7_obj_to_atom");

    // TODO: not yet handling lists or vectors (in Max, these become atom arrays)
    // TODO: not yet handling hashtables (in Max, these become dicts)
    
    // booleans are cast to floats 
    if( s7_is_boolean(s7_obj) ){
        post("creating Pd 1 or 0 float from s7 boolean");
        SETFLOAT(atom, (float)s7_boolean(s7, s7_obj));  
    }
    else if( s7_is_integer(s7_obj)){
        post("creating int atom, %i", s7_integer(s7_obj));
        SETFLOAT(atom, s7_integer(s7_obj));
    }
    else if( s7_is_real(s7_obj)){
        post("creating float atom, %.2f", s7_real(s7_obj));
        SETFLOAT(atom, s7_real(s7_obj));
    }
    else if( s7_is_symbol(s7_obj) ){
        // both s7 symbols and strings are converted to symbols
        post("creating symbol atom, %s", s7_symbol_name(s7_obj));
        SETSYMBOL(atom, gensym( s7_symbol_name(s7_obj)));
    }
    else if( s7_is_string(s7_obj) ){
        post("creating symbol atom from string, %s", s7_string(s7_obj));
        SETSYMBOL(atom, gensym( s7_string(s7_obj)));
    }
    else if( s7_is_character(s7_obj) ){
        post("creating symbol atom from character");
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

// get a opd struct pointer from the s7 environment pointer
t_s4pd *get_pd_obj(s7_scheme *s7){
    // get our max object by reading the max pointer from the scheme environment
    uintptr_t s4pd_ptr_from_s7 = (uintptr_t)s7_integer( s7_name_to_value(s7, "pd-obj") );
    t_s4pd *s4pd_ptr = (t_s4pd *)s4pd_ptr_from_s7;
    return s4pd_ptr;
}

// function to send generic output out an outlet
static s7_pointer s7_pd_output(s7_scheme *s7, s7_pointer args){
    post("s7_pd_output, args: %s", s7_object_to_c_string(s7, args));
    
    // all added functions have this form, args is a list, s7_car(args) is the first arg, etc 
    int outlet_num = (int) s7_real( s7_car(args) );
    t_s4pd *x = get_pd_obj(s7);

    // check if outlet number exists
    if( outlet_num > x->num_outlets || outlet_num < 0 ){
        post("ERROR: invalid outlet number %i", outlet_num);
        return s7_nil(s7);
    }
    s7_pointer s7_out_val = s7_cadr(args);
    t_symbol *msg_sym;  // the first symbol for the outlet_anything message 
    t_atom output_atom; 
    int err;

    post("  s7_out_val: %s", s7_object_to_c_string(s7, s7_out_val));

    // bools and all numbers become pd floats 
    if( s7_is_real(s7_out_val) || s7_is_boolean(s7_out_val) ){
        err = s7_obj_to_atom(s7, s7_out_val, &output_atom);
        if( err ){
          post("error outputing %s", s7_object_to_c_string(s7, s7_out_val));
        }else{
          outlet_anything( x->outlets[outlet_num], gensym("float"), 1, &output_atom);
        }
    }
    // symbols, keywords, chars, and strings all become pd symbols
    // to the message type IS the symbol
    // XXX: not working for quoted symbols for some reason
    else if( s7_is_string(s7_out_val) || s7_is_symbol(s7_out_val) || s7_is_character(s7_out_val) ){
        post(" - symbol output");
        // note that symbol catches keywords as well
        err = s7_obj_to_atom(s7, s7_out_val, &output_atom);
        if( err ){
            post("error outputing %s", s7_object_to_c_string(s7, s7_out_val));
        }else{
            outlet_symbol(x->outlets[outlet_num], atom_getsymbol(&output_atom)); 
        }
    }else{
        post("error outputing %s", s7_object_to_c_string(s7, s7_out_val));
    }
    return s7_nil(s7);

    /*
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
                s7_obj_to_max_atom(s7, s7_list_ref(s7, s7_out_val, i), &out_list[i]);
            }
            // add the symbol "list" as the first item for the message output
            outlet_anything( x->outlets[outlet_num], gensym("list"), length, out_list);     
        }
        else {
            //post("list starting with a symbol");     
            // build the atom list, starting from the second item 
            for(int i=1; i<length; i++){
                s7_obj_to_max_atom(s7, s7_list_ref(s7, s7_out_val, i), &out_list[i - 1]);
            }
            // convert the first item to use as symbol for message
            err = s7_obj_to_max_atom(s7, first, &output_atom); 
            outlet_anything( x->outlets[outlet_num], atom_getsym(&output_atom), length - 1, out_list);     
        }
    }
    // vectors are supported for bool, int, float only
    else if( s7_is_vector(s7_out_val) && s7_vector_length(s7_out_val) > 0 ){
        t_atom out_list[MAX_ATOMS_PER_OUTPUT_LIST];
        int length = s7_vector_length(s7_out_val);
        for(int i=0; i<length; i++){
            // if invalid type, return with error
            s7_pointer *item = s7_vector_ref(s7, s7_out_val, i);
            if( s7_is_number(item) || s7_is_boolean(item)){
                s7_obj_to_max_atom(s7, item, &out_list[i]);
            }else{
                error("s4m: Vector output only supported for ints, floats, & booleans");
                return s7_nil(s7);
            }
        }
        // didn't hit an invalid type, we can output the list
        outlet_anything( x->outlets[outlet_num], gensym("list"), length, out_list);     
    } 
    // unhandled output type, post an error
    else{
        error("s4m: Unhandled output type %s", s7_object_to_c_string(s7, s7_out_val));
    }
    // returns nil so that the console is not chatting on every output message
    return s7_nil(s7);
    */
}


void s4pd_bang(t_s4pd *x){
    (void)x; // silence unused variable warning
    post("s4pd received bang...");
    
    // lets run us some scheme...
    //char * code_1 = "(define (hello-world) :hello-the-world)";
    //s4pd_s7_eval_string(x, code_1);

    //char * code_2 = "(hello-world)";
    //s4pd_s7_eval_string(x, code_2);

    // send a float out the outlet
    //outlet_float(x->x_obj.ob_outlet, 123.123);
    outlet_float(x->outlets[0], 0.0);
    outlet_float(x->outlets[1], 1.0);
}  

void s4pd_message(t_s4pd *x, t_symbol *s, int argc, t_atom *argv){
    post("s4pd_message() argc: %i", argc);
    t_atom *ap;
    s7_pointer s7_args = s7_nil(x->s7); 
    // loop through the args backwards to build the cons list 
    for(int i = argc-1; i >= 0; i--) {
        ap = argv + i;
        s7_args = s7_cons(x->s7, atom_to_s7_obj(x->s7, ap), s7_args); 
    }
    //post("  - s7 args: %s", s7_object_to_c_string(x->s7, s7_args) );
    // use first symbol as function and call 
    s4pd_s7_call(x, s7_name_to_value(x->s7, s->s_name), s7_args);

    // how we do it in s4m in case we want to adopt this later:
    // add the first message to the arg list (it's always a symbol)
    // call the s7 eval function, sending in all args as an s7 list
    //s7_args = s7_cons(x->s7, s7_make_symbol(x->s7, s->s_name), s7_args); 
    //s4pd_s7_call(x, s7_name_to_value(x->s7, "s4m-eval"), s7_args);
}

void s4pd_init_s7(t_s4pd *x){
    post("s4pd_init_s7");
    // start the S7 interpreter 
    x->s7 = s7_init();
    s7_define_function(x->s7, "pd-output", s7_pd_output, 2, 0, false, "(pd-output 1 99) sends value 99 out outlet 1");

    // make the address of this object available in scheme as "pd-obj" so that 
    // scheme functions can get access to our C functions
    uintptr_t pd_obj_ptr = (uintptr_t)x;
    s7_define_variable(x->s7, "pd-obj", s7_make_integer(x->s7, pd_obj_ptr));  
    post("init complete");
}


void *s4pd_new(t_symbol *s, int argc, t_atom *argv){  
    post("s4pd_new()");
    t_s4pd *x = (t_s4pd *) pd_new (s4pd_class);

    // set up default vars
    x->log_return_values = true;
    x->log_null = false;
    x->num_outlets = 1;
    x->filename = gensym("");

    // if args are given, they are: outlets, filename
    switch(argc){
      default:
      case 2:
        x->filename = atom_getsymbol(argv+2);
      case 1:
        x->num_outlets = atom_getint(argv);
    }
    post(" - outlets: %i filename: %s", x->num_outlets, x->filename->s_name);
    // make the outlets
    for(int i = 0; i < x->num_outlets; i++){
      x->outlets[i] = outlet_new(&x->x_obj, 0);
    }

    s4pd_init_s7(x); 
    return (void *)x;  
}  
 
void s4pd_setup(void) {  
    s4pd_class = class_new(gensym("s4pd"),  
        (t_newmethod)s4pd_new, 
        NULL, 
        sizeof(t_s4pd), 
        CLASS_DEFAULT, 
        A_GIMME,        // allow dynamic number of arguments
        0);  

    class_addbang(s4pd_class, s4pd_bang);  
    class_addmethod(s4pd_class, (t_method)s4pd_log_null, gensym("log-null"), A_DEFFLOAT, 0);
    class_addmethod(s4pd_class, (t_method)s4pd_reset, gensym("reset"), 0);
    class_addanything(s4pd_class, (t_method)s4pd_message);
}

void s4pd_reset(){
    post("reset message received!");
}

// a method that takes a float and does something
void s4pd_log_null(t_s4pd *x, t_floatarg arg){
    post("log_null()");
    x->log_null = (int) arg == 0 ? false : true;
    post("x.log_null now: %i", x->log_null);
}

// hook to allow user to alter how results get logged to the console
void s4pd_post_s7_res(t_s4pd *x, s7_pointer res) {
    char *log_out = s7_object_to_c_string(x->s7, res);
    post("s4pd> %s", log_out);
}


// eval string  with error logging
void s4pd_s7_eval_string(t_s4pd *x, char *string_to_eval){
    post("s4pd_s7_eval_string() %s", string_to_eval);
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
        if(x->log_return_values) s4pd_post_s7_res(x, res);
    }
}

// call s7_call, with error logging
void s4pd_s7_call(t_s4pd *x, s7_pointer funct, s7_pointer args){
    post("s4pd_s7_call()");
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
        if(x->log_return_values) s4pd_post_s7_res(x, res);
    }
}
/*
// call s7_load, with error logging
void s4pd_s7_load(t_s4pd *x, char *full_path){
    // post("s4pd_s7_load() %s", full_path);
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
        object_error((t_object *)x, "s4pd Error loading %s: %s", full_path, msg);
        free(msg);
    }else{
        // we don't run this in production as the res printed is the last line of
        // the file loaded, which looks weird to the user
    }
}
*/
