
#include <bake/bake.h>

#define OBJ_DIR ".corto/obj"

/* -- Mappings */
static 
char* src_to_obj(
    bake_language *l,     
    bake_project *p, 
    const char *in,
    void *ctx) 
{
    char *result = malloc(strlen(in) + strlen(OBJ_DIR) + 2);
    sprintf(result, OBJ_DIR "/%s", in + 4); /* strip 'src/' */
    char *ext = strrchr(result, '.');
    strcpy(ext + 1, "o");
    return result;
}

char* src_to_dep(
    bake_language *l,       
    bake_project *p, 
    const char *in,
    void *ctx) 
{
    printf("src to dep (%s)\n", in);
    return NULL;
}

char* obj_to_dep(
    bake_language *l,
    bake_project *p,
    const char *in,
    void *ctx) 
{
    printf("obj to dep (%s)\n", in);
    return NULL;
}


/* -- Actions */
static
void gen_source(
    bake_language *l,
    bake_project *p, 
    bake_config *c,    
    char *source, 
    char *target, 
    void *ctx)
{

}

static
void generate_deps(
    bake_language *l,
    bake_project *p, 
    bake_config *c,
    char *source, 
    char *target, 
    void *ctx)
{

}

static
void compile_src(
    bake_language *l,
    bake_project *p, 
    bake_config *c,
    char *source, 
    char *target, 
    void *ctx)
{
    corto_buffer cmd = CORTO_BUFFER_INIT;
    corto_buffer_appendstr(
        &cmd, "gcc -Wall -pedantic -Werror -fPIC -std=c99 -D_XOPEN_SOURCE=600");

    char *building_macro = corto_asprintf(" -DBUILDING_%s", p->id);
    strupper(building_macro);
    char *ptr, ch;
    for (ptr = building_macro; (ch = *ptr); ptr++) {
        if (ch == '/') {
            *ptr = '_';
        }
    }
    corto_buffer_appendstr(&cmd, building_macro);
    free(building_macro);

    if (c->symbols) {
        corto_buffer_appendstr(&cmd, " -g");
    }
    if (!c->debug) {
        corto_buffer_appendstr(&cmd, " -DNDEBUG");
    }
    if (c->optimizations) {
        corto_buffer_appendstr(&cmd, " -O3");
    } else {
        corto_buffer_appendstr(&cmd, " -O0");
    }

    corto_buffer_append(&cmd, " -I $CORTO_TARGET/include/corto/$CORTO_VERSION");

    if (strcmp(corto_getenv("CORTO_TARGET"), corto_getenv("CORTO_HOME"))) {
        corto_buffer_append(&cmd, " -I $CORTO_HOME/include/corto/$CORTO_VERSION");
    }

    if (strcmp("/usr/local", corto_getenv("CORTO_HOME")) && strcmp("/usr/local", corto_getenv("CORTO_TARGET"))) {
        corto_buffer_append(&cmd, " -I /usr/local/include/corto/$CORTO_VERSION");
    }

    corto_buffer_append(&cmd, " -I. -c %s -o %s", source, target);

    char *cmdstr = corto_buffer_str(&cmd);
    l->exec(cmdstr);
    free(cmdstr);
}

static
void obj_deps(
    bake_language *l,
    bake_project *p, 
    bake_config *c,    
    char *source, 
    char *target, 
    void *ctx)
{

}

static
void link_binary(
    bake_language *l,
    bake_project *p, 
    bake_config *c,    
    char *source, 
    char *target, 
    void *ctx)
{
    corto_buffer cmd = CORTO_BUFFER_INIT;
    corto_buffer_appendstr(
        &cmd, "gcc -Wall -pedantic -Werror -fPIC -std=c99 -D_XOPEN_SOURCE=600");

    if (p->kind == BAKE_LIBRARY) {
        corto_buffer_appendstr(&cmd, " --shared");
    }

    if (c->optimizations) {
        corto_buffer_appendstr(&cmd, " -O3");
    } else {
        corto_buffer_appendstr(&cmd, " -O0");
    }

    corto_buffer_append(&cmd, " %s", source);

    corto_buffer_append(&cmd, " -o %s", target);

    char *cmdstr = corto_buffer_str(&cmd);
    l->exec(cmdstr);
    free(cmdstr);
}

static
int16_t artefact_name(
    bake_language *l,
    bake_filelist *fl,
    bake_project *p)
{
    char *base = strrchr(p->id, '/');
    if (!base) {
        base = p->id;
    } else {
        base ++;
    }

    if (p->kind == BAKE_LIBRARY) {
        corto_id libname;
        sprintf(libname, "lib%s.so", base);
        return fl->set(libname);
    } else {
        return fl->set(base);
    }
}

/* -- Rules */
int bakemain(bake_language *l) {

    base_init("driver/bake/c");

    /* Create pattern that matches generated source files */
    l->pattern("gen-sources", ".corto/gen//*.c");

    /* Generate rule for dynamically generating source for definition file */
    l->rule("gen-code", "$DEFINITION", l->target_pattern("$gen-sources"), gen_source);

    /* Create pattern that matches source files */
    l->pattern("sources", "src//*.c");

    /* Create rule for dynamically generating dep files from source files */
    l->rule("deps", "$sources", l->target_map(src_to_dep), generate_deps);

    /* Create rule for dynamically generating object files from source files */
    l->rule("objects", "$sources", l->target_map(src_to_obj), compile_src);

    /* Create rule for dynamically generating dependencies for every object in
     * $objects, using the generated dependency files. */
    l->dependency_rule("$objects", "$deps", l->target_map(obj_to_dep), obj_deps);

    /* Create rule for creating binary from objects */
    l->rule("ARTEFACT", "$objects", l->target_pattern(NULL), link_binary);

    /* Set callback for generating artefact name(s) */
    l->artefact(artefact_name);

    return 0;
}
