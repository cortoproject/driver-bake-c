
#include <bake/bake.h>

#define OBJ_DIR ".bake_cache/obj"

#define LOCAL_INCLUDE_ALIAS "$(CORTO_INCLUDE)"
#define LOCAL_INCLUDE_DIR_PATH "$BAKE_TARGET/include/corto/$BAKE_VERSION/%s"

static
char* get_short_name(
    char *package)
{
    char *result = strrchr(package, '/');
    if (!result) {
        result = package;
    } else {
        result ++;
    }
    return result;
}

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
    bool c4cpp = !strcmp(p->get_attr_string("c4cpp"), "true");
    if (p->managed) {
        corto_buffer cmd = CORTO_BUFFER_INIT;
        char *shortName = get_short_name(p->id);

        if (p->model) {
            corto_buffer_append(
                &cmd,
                "corto pp project.json %s --scope %s --lang %s",
                p->model,
                p->id,
                p->language);
        } else {
            corto_buffer_append(
                &cmd,
                "corto pp -g c/interface -g c/project");
        }

        corto_buffer_append(
            &cmd,
            " --name %s --prefix %s --attr c=src --attr cpp=src --attr h=include --attr hpp=include --attr hidden=.bake_cache/gen",
            p->id,
            shortName);

        if (!p->public) {
            corto_buffer_append(&cmd, " --attr local=true");
        }

        if (p->kind != BAKE_PACKAGE) {
            corto_buffer_append(&cmd, " --attr app=true");
        }

        if (c4cpp) {
            corto_buffer_append(&cmd, " --attr c4cpp=true");
        }

        if (corto_ll_count(p->use)) {
            corto_buffer imports = CORTO_BUFFER_INIT;
            corto_buffer_append(&imports, " --use ");
            corto_iter it = corto_ll_iter(p->use);
            int count = 0;
            while (corto_iter_hasNext(&it)) {
                char *use = corto_iter_next(&it);
                if (!strcmp(use, strarg("%s/c", p->id))) {
                  /* No need to explicitly add own /c package */
                  continue;
                }
                if (count) {
                    corto_buffer_append(&imports, ",");
                }
                corto_buffer_appendstr(&imports, use);
                count ++;
            }
            if (count) {
                char *importStr = corto_buffer_str(&imports);
                corto_buffer_appendstr(&cmd, importStr);
            }
        }

        char *cmdstr = corto_buffer_str(&cmd);
        l->exec(cmdstr);
        free(cmdstr);
    }
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
    bool c4cpp = !strcmp(p->get_attr_string("c4cpp"), "true");

    if (!c4cpp) {
        corto_buffer_appendstr(
            &cmd, "gcc -Wall -fPIC -std=c99 -D_XOPEN_SOURCE=600");
    } else {
        corto_buffer_appendstr(
            &cmd, "g++ -Wall -std=c++0x -fPIC -Wno-write-strings");
    }

    corto_buffer_append(&cmd, " -DPACKAGE_ID=\"%s\"", p->id);

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

    char *etc_macro = corto_asprintf(" -D%s_ETC", p->id);
    strupper(etc_macro);
    for (ptr = etc_macro; (ch = *ptr); ptr++) {
        if (ch == '/') {
            *ptr = '_';
        }
    }
    if (p->public) {
        corto_buffer_append(&cmd, "%s=corto_locate(PACKAGE_ID,NULL,CORTO_LOCATION_ETC)", etc_macro);
    } else {
        corto_buffer_append(&cmd, "%s=\"etc\"", etc_macro);
    }
    free(etc_macro);

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
    if (c->strict) {
        corto_buffer_appendstr(&cmd, " -Werror -pedantic");
    }

    bake_project_attr *include_attr = p->get_attr("include");
    if (include_attr) {
        corto_iter it = corto_ll_iter(include_attr->is.array);
        while (corto_iter_hasNext(&it)) {
            bake_project_attr *include = corto_iter_next(&it);
            if (strcmp(include->is.string, LOCAL_INCLUDE_ALIAS)) {
                corto_buffer_append(&cmd, " -I%s", include->is.string);
            } else {
                /* Support $(CORTO_INCLUDE) include path */
                corto_buffer_append(&cmd, " -I"LOCAL_INCLUDE_DIR_PATH, p->id);
            }
        }
    }

    corto_buffer_append(&cmd, " -I $BAKE_TARGET/include/corto/$BAKE_VERSION");

    if (strcmp(corto_getenv("BAKE_TARGET"), corto_getenv("BAKE_HOME"))) {
        corto_buffer_append(&cmd, " -I $BAKE_HOME/include/corto/$BAKE_VERSION");
    }

    if (strcmp("/usr/local", corto_getenv("BAKE_HOME")) && strcmp("/usr/local", corto_getenv("BAKE_TARGET"))) {
        corto_buffer_append(&cmd, " -I /usr/local/include/corto/$BAKE_VERSION");
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
    bool c4cpp = !strcmp(p->get_attr_string("c4cpp"), "true");

    corto_buffer cmd = CORTO_BUFFER_INIT;
    if (!c4cpp) {
        corto_buffer_appendstr(
            &cmd, "gcc -Wall -fPIC");
    } else {
        corto_buffer_appendstr(
            &cmd, "g++ -Wall -fPIC");
    }

    if (p->kind == BAKE_PACKAGE) {
        corto_buffer_appendstr(&cmd, " --shared -Wl,-z,defs");
    }

    if (c->optimizations) {
        corto_buffer_appendstr(&cmd, " -O3");
    } else {
        corto_buffer_appendstr(&cmd, " -O0");
    }

    if (c->strict) {
        corto_buffer_appendstr(&cmd, " -Werror -pedantic");
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

    bake_project_attr *libpath_attr = p->get_attr("libpath");
    if (libpath_attr) {
        corto_iter it = corto_ll_iter(libpath_attr->is.array);
        while (corto_iter_hasNext(&it)) {
            bake_project_attr *lib = corto_iter_next(&it);
            corto_buffer_append(&cmd, " -L%s", lib->is.string);
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
    char *base = get_short_name(p->id);

    if (p->kind == BAKE_PACKAGE) {
        return corto_asprintf("lib%s.so", base);
    } else {
        return corto_strdup(base);
    }
}

static void clean(
    bake_language *l,
    bake_project *p)
{
    if (p->managed) {
        p->clean("include/_project.h");
        if (p->model) {
            p->clean("include/_type.h");
            p->clean("include/_load.h");
            p->clean("include/_interface.h");
            if (!p->public || p->kind != BAKE_PACKAGE) {
                p->clean("include/_api.h");
            }
        }
    }
}

bool project_is_managed(bake_project *p) {
    return p->managed;
}

bool project_has_model_and_public_and_package(bake_project *p) {
    return p->model != NULL && p->public && p->kind == BAKE_PACKAGE;
}

/* -- Rules */
int bakemain(bake_language *l) {

    base_init("driver/bake/c");

    /* Create pattern that matches generated source files */
    l->pattern("gen-sources", ".bake_cache/gen//*.c|*.cpp|*.cxx");

    l->pattern("gen-sources-2", ".bake_cache/gen//*.c|*.cpp|*.cxx");

    /* Create pattern that matches files in generated binding API */
    l->pattern("api-sources", "c/src//*.c|*.cpp|*.cxx");

    /* Generate rule for dynamically generating source for definition file */
    l->rule("GENERATED-SOURCES", "$MODEL,project.json", l->target_pattern("$gen-sources,$api-sources"), gen_source);

    /* Create pattern that matches source files */
    l->pattern("SOURCES", "//*.c|*.cpp|*.cxx");

    /* Create rule for dynamically generating dep files from source files */
    l->rule("deps", "$SOURCES", l->target_map(src_to_dep), generate_deps);

    /* Create rule for dynamically generating object files from source files */
    l->rule("objects", "$SOURCES,$gen-sources-2", l->target_map(src_to_obj), compile_src);

    /* Create rule for dynamically generating dependencies for every object in
     * $objects, using the generated dependency files. */
    l->dependency_rule("$objects", "$deps", l->target_map(obj_to_dep), obj_deps);

    /* Create rule for creating binary from objects */
    l->rule("ARTEFACT", "$objects", l->target_pattern(NULL), link_binary);

    /* Add conditions to rules that are evaluated per project */
    l->condition("GENERATED-SOURCES", project_is_managed);
    l->condition("api-sources", project_has_model_and_public_and_package);

    /* Callback that specifies files to clean */
    l->clean(clean);

    /* Set callback for generating artefact name(s) */
    l->artefact(artefact_name);

    return 0;
}
