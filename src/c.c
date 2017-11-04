
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
    sprintf(result, OBJ_DIR "/%s", in);
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
    corto_buffer cmd = CORTO_BUFFER_INIT;

    corto_buffer_append(
        &cmd, 
        "corto pp %s --scope %s --name %s --attr c=src --attr h=include", 
        source, 
        p->id, 
        p->id);

    if (corto_ll_size(p->use)) {
        corto_buffer imports = CORTO_BUFFER_INIT;
        corto_buffer_append(&imports, " --use ");
        corto_iter it = corto_ll_iter(p->use);
        int count = 0;
        while (corto_iter_hasNext(&it)) {
            char *use = corto_iter_next(&it);
            char *lastElem = strrchr(use, '/');
            if (!lastElem || strcmp(lastElem, "/c")) {
                if (count) {
                    corto_buffer_append(&imports, ",");
                }
                corto_buffer_appendstr(&imports, use);
                count ++;
            }
        }
        if (count) {
            char *importStr = corto_buffer_str(&imports);
            corto_buffer_appendstr(&cmd, importStr);
        }
    }

    if (!p->public) {
        corto_buffer_append(&cmd, " --attr local=true");
    }

    corto_buffer_append(&cmd, " --lang %s", p->language);

    char *cmdstr = corto_buffer_str(&cmd);
    l->exec(cmdstr);
    free(cmdstr);
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

    bake_project_attr *include_attr = p->get_attr("include");
    if (include_attr) {
        corto_iter it = corto_ll_iter(include_attr->is.array);
        while (corto_iter_hasNext(&it)) {
            bake_project_attr *include = corto_iter_next(&it);
            corto_buffer_append(&cmd, " -I%s", include->is.string);
        }
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

    if (p->kind == BAKE_PACKAGE) {
        corto_buffer_appendstr(&cmd, " --shared -Wl,-z,defs");
    }

    if (c->optimizations) {
        corto_buffer_appendstr(&cmd, " -O3");
    } else {
        corto_buffer_appendstr(&cmd, " -O0");
    }

    corto_buffer_append(&cmd, " %s", source);

    bake_project_attr *lib_attr = p->get_attr("lib");
    if (lib_attr) {
        corto_iter it = corto_ll_iter(lib_attr->is.array);
        while (corto_iter_hasNext(&it)) {
            bake_project_attr *lib = corto_iter_next(&it);
            corto_buffer_append(&cmd, " -l%s", lib->is.string);
        }
    }

    corto_iter it = corto_ll_iter(p->link);
    while (corto_iter_hasNext(&it)) {
        char *lib = corto_iter_next(&it);
        corto_buffer_append(&cmd, " %s", lib);
    }

    corto_buffer_append(&cmd, " -o %s", target);

    char *cmdstr = corto_buffer_str(&cmd);
    l->exec(cmdstr);
    free(cmdstr);
}

static
char* artefact_name(
    bake_language *l,
    bake_project *p)
{
    char *base = strrchr(p->id, '/');
    if (!base) {
        base = p->id;
    } else {
        base ++;
    }

    if (p->kind == BAKE_PACKAGE) {
        return corto_asprintf("lib%s.so", base);
    } else {
        return corto_strdup(base);
    }
}

/* -- Rules */
int bakemain(bake_language *l) {

    base_init("driver/bake/c");

    /* Create pattern that matches generated source files */
    l->pattern("gen-sources", ".corto/gen//*.c");

    /* Generate rule for dynamically generating source for definition file */
    l->rule("GENERATED-SOURCES", "$MODEL", l->target_pattern("$gen-sources"), gen_source);

    /* Create pattern that matches source files */
    l->pattern("SOURCES", "//*.c");

    /* Create rule for dynamically generating dep files from source files */
    l->rule("deps", "$SOURCES", l->target_map(src_to_dep), generate_deps);

    /* Create rule for dynamically generating object files from source files */
    l->rule("objects", "$SOURCES,$gen-sources", l->target_map(src_to_obj), compile_src);

    /* Create rule for dynamically generating dependencies for every object in
     * $objects, using the generated dependency files. */
    l->dependency_rule("$objects", "$deps", l->target_map(obj_to_dep), obj_deps);

    /* Create rule for creating binary from objects */
    l->rule("ARTEFACT", "$objects", l->target_pattern(NULL), link_binary);

    /* Set callback for generating artefact name(s) */
    l->artefact(artefact_name);

    return 0;
}
