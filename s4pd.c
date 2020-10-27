#include "m_pd.h"  
#include "string.h"
#include "s7.h"
 
static t_class *s4pd_class;  
 
typedef struct _s4pd {  
    t_object x_obj;
    s7_scheme *s7;              // pointer to the s7 instance
    bool log_return_values;     // whether to automatically post return values from S7 interpreter to console
} t_s4pd;  
   
void s4pd_s7_load(t_s4pd *x, char *full_path);
void s4pd_s7_call(t_s4pd *x, s7_pointer funct, s7_pointer args);
void s4pd_s7_eval_string(t_s4pd *x, char *string_to_eval);
void s4pd_post_s7_res(t_s4pd *x, s7_pointer res);
void s4pd_s7_eval_string(t_s4pd *x, char *string_to_eval);


void s4pd_bang(t_s4pd *x){
    (void)x; // silence unused variable warning
    post("s4pd received bang...");
    
    // lets run us some scheme...
    char * code_1 = "(define (hello-world) :hello-the-world)";
    s4pd_s7_eval_string(x, code_1);

    char * code_2 = "(hello-world)";
    s4pd_s7_eval_string(x, code_2);
}  

void s4pd_init_s7(t_s4pd *x){
    post("s4pd_init_s7");
    // start the S7 interpreter 
    x->s7 = s7_init();
    post("init complete");
}


void *s4pd_new(void){  
    post("s4pd_new()");
    t_s4pd *x = (t_s4pd *) pd_new (s4pd_class);
    // set up private vars
    x->log_return_values = true;
    s4pd_init_s7(x); 
    return (void *)x;  
}  
 
void s4pd_setup(void) {  
    s4pd_class = class_new(gensym("s4pd"),  (t_newmethod)s4pd_new, NULL, sizeof(t_s4pd), CLASS_DEFAULT, 0);  
    class_addbang(s4pd_class, s4pd_bang);  
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

/*
// call s7_call, with error logging
void s4pd_s7_call(t_s4pd *x, s7_pointer funct, s7_pointer args){
    //post("s4pd_s7_call()");
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
        object_error((t_object *)x, "s4pd Error: %s", msg);
        free(msg);
    }else{
        if(x->log_return_values) s4pd_post_s7_res(x, res);
    }
}

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
