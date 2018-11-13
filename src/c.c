
#include <bake/bake.h>

#define OBJ_DIR ".bake_cache/obj"

static
char* get_short_name(
    const char *package)
{
    char *result = strrchr(package, '/');
    if (!result) {
        result = (char*)package;
    } else {
        result ++;
    }

    char *ptr = strrchr(package, '.');
    if (ptr) {
        result = ptr + 1;
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
    const char *cfg = p->cfg->id;
    char *result = malloc(strlen(in) + strlen(OBJ_DIR) + strlen(CORTO_PLATFORM_STRING) + strlen(cfg) + 4);
    sprintf(result, OBJ_DIR "/%s-%s/%s", CORTO_PLATFORM_STRING, cfg, in);
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
        char *scope = p->id;
        char *scope_attr = p->get_attr_string("scope");

        if (strlen(scope_attr)) {
            scope = scope_attr;
        }

        if (p->model) {
            corto_buffer_append(
                &cmd,
                "corto pp project.json %s --scope %s --lang %s",
                p->model,
                scope,
                p->language);
        } else {
            corto_buffer_append(
                &cmd,
                "corto pp -g c/interface -g c/project");
        }

        corto_buffer_append(
            &cmd,
            " --name %s --attr c=src --attr cpp=src --attr h=include --attr hpp=include --attr hidden=.bake_cache/gen",
            p->id);

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
                if (!strcmp(use, strarg("%s/c", p->id)) ||
                    !strcmp(use, strarg("%s/cpp", p->id)))
                {
                    /* Should not add own generated language packages because they
                     * may not yet exist */
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

        if (corto_ll_count(p->use_private)) {
            corto_buffer imports = CORTO_BUFFER_INIT;
            corto_buffer_append(&imports, " --use-private ");
            corto_iter it = corto_ll_iter(p->use_private);
            int count = 0;
            while (corto_iter_hasNext(&it)) {
                char *use = corto_iter_next(&it);
                if (!strcmp(use, strarg("%s/c", p->id)) ||
                    !strcmp(use, strarg("%s/cpp", p->id)))
                {
                    /* Should not add own generated language packages because they
                     * may not yet exist */
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
bool is_darwin(void)
{
    if (strcmp(CORTO_OS_STRING, "darwin")) {
        return false;
    }
    return true;
}

static
const char *cc(
    bake_project *p)
{
    bool c4cpp = !strcmp(p->get_attr_string("c4cpp"), "true");
    if (!is_darwin()) {
        if (c4cpp) {
            return "g++";
        } else {
            return "gcc";
        }
    } else {
        if (c4cpp) {
            return "clang++";
        } else {
            return "clang";
        }
    }
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
    char *ext = strrchr(source, '.');
    bool isCpp = false;
    bool c4cpp = !strcmp(p->get_attr_string("c4cpp"), "true");
    if (c4cpp || (ext && strcmp(ext, ".c"))) {
        /* If extension is not c, it is a C++ file */
        isCpp = true;
    }

    corto_buffer_append(&cmd, "%s -Wall -fPIC -fno-stack-protector", cc(p));

    if (isCpp) {
        corto_buffer_appendstr(&cmd, " -std=c++0x -Wno-write-strings");
    } else {
        corto_buffer_appendstr(&cmd, " -std=c99 -D_XOPEN_SOURCE=600");
    }

    corto_buffer_append(&cmd, " -DPACKAGE_ID=\"%s\"", p->id);

    char *building_macro = corto_asprintf(" -D%s_IMPL", p->id);
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
        corto_buffer_append(&cmd, "%s=corto_locate(PACKAGE_ID,NULL,CORTO_LOCATE_ETC)", etc_macro);
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
        corto_buffer_appendstr(&cmd, " -O3 -flto");
    } else {
        corto_buffer_appendstr(&cmd, " -O0");
    }
    if (c->strict) {
        corto_buffer_appendstr(&cmd, " -Werror -Wextra -pedantic");
    }

    if (!isCpp) {
        /* CFLAGS for c projects */
        bake_project_attr *flags_attr = p->get_attr("cflags");
        if (flags_attr) {
            corto_iter it = corto_ll_iter(flags_attr->is.array);
            while (corto_iter_hasNext(&it)) {
                bake_project_attr *flag = corto_iter_next(&it);
                corto_buffer_append(&cmd, " %s", flag->is.string);
            }
        }
    } else {
        /* CXXFLAGS for c4cpp projects */
        bake_project_attr *flags_attr = p->get_attr("cxxflags");
        if (flags_attr) {
            corto_iter it = corto_ll_iter(flags_attr->is.array);
            while (corto_iter_hasNext(&it)) {
                bake_project_attr *flag = corto_iter_next(&it);
                corto_buffer_append(&cmd, " %s", flag->is.string);
            }
        }
    }

    bake_project_attr *include_attr = p->get_attr("include");
    if (include_attr) {
        corto_iter it = corto_ll_iter(include_attr->is.array);
        while (corto_iter_hasNext(&it)) {
            bake_project_attr *include = corto_iter_next(&it);
            char* file = include->is.string;
            corto_buffer_append(&cmd, " -I%s", file);
        }
    }

    corto_buffer_append(&cmd, " -I %s/include", c->rootpath);

    if (strcmp(corto_getenv("BAKE_TARGET"), corto_getenv("BAKE_HOME"))) {
        corto_buffer_append(&cmd, " -I %s/include", c->homepath);
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
const char* lib_map(
    const char *lib)
{
    /* On darwin, librt does not exist */
    if (!strcmp(lib, "rt")) {
        if (is_darwin()) {
            return NULL;
        }
    }

    return lib;
}

static
char* find_static_lib(
    bake_project *p,
    bake_config *c,
    const char *lib)
{
    int ret;

    /* Find static library in configuration libpath */
    char *file = corto_asprintf("%s/lib%s.a", c->libpath, lib);
    if ((ret = corto_file_test(file)) == 1) {
        return file;
    } else if (ret != 0) {
        free(file);
        corto_error("could not access '%s'", file);
        return NULL;
    }

    free(file);

    /* If static library is not found in configuration, try libpath */
    bake_project_attr *libpath_attr = p->get_attr("libpath");
    if (libpath_attr) {
        corto_iter it = corto_ll_iter(libpath_attr->is.array);
        while (corto_iter_hasNext(&it)) {
            bake_project_attr *lib = corto_iter_next(&it);
            file = corto_asprintf("%s/lib%s.a", lib->is.string, lib);

            if ((ret = corto_file_test(file)) == 1) {
                return file;
            } else if (ret != 0) {
                free(file);
                corto_error("could not access '%s'", file);
                return NULL;
            }

            free(file);
        }
    }

    return NULL;
}

static
bool is_dylib(
    bake_project *p)
{
    if (is_darwin()) {
        bool dylib = false;
        bake_project_attr *dylib_attr = p->get_attr("dylib");
        if (dylib_attr) {
            dylib = dylib_attr->is.boolean;
        }
        return dylib;
    } else {
        return false;
    }
}

static
char *project_name(
    const char *project_id)
{
    char *result = NULL;
    char *id = corto_strdup(project_id);
    char *ptr, ch;
    for (ptr = id; (ch = *ptr); ptr++) {
        if (ch == '/') {
            *ptr = '_';
        }
    }

    return id;
}

static
char* artefact_name(
    bake_language *l,
    bake_project *p)
{
    char *result;
    char *id = project_name(p->id);
    if (p->kind == BAKE_PACKAGE) {
        bool link_static = p->get_attr_bool("static_artefact");

        if (link_static) {
            result = corto_asprintf("lib%s.a", id);
        } else {
            if (is_dylib(p)) {
                result = corto_asprintf("lib%s.dylib", id);
            } else {
                result = corto_asprintf("lib%s.so", id);
            }
        }
    } else {
        result = corto_strdup(id);
    }
    free(id);
    return result;
}

static
void link_dynamic_binary(
    bake_language *l,
    bake_project *p,
    bake_config *c,
    char *source,
    char *target,
    void *ctx)
{
    corto_buffer cmd = CORTO_BUFFER_INIT;
    bool hide_symbols = false;
    corto_ll static_object_paths = NULL;

    corto_buffer_append(&cmd, "%s -Wall -fPIC", cc(p));

    if (p->kind == BAKE_PACKAGE) {
        if (p->managed && !is_darwin()) {
            corto_buffer_appendstr(&cmd, " -Wl,-fvisibility=hidden");
            hide_symbols = true;
        }
        corto_buffer_appendstr(&cmd, " -fno-stack-protector --shared");
        if (!is_darwin()) {
            corto_buffer_appendstr(&cmd, " -Wl,-z,defs");
        }
    }

    if (c->optimizations) {
        corto_buffer_appendstr(&cmd, " -O3");
    } else {
        corto_buffer_appendstr(&cmd, " -O0");
    }

    if (c->strict) {
        corto_buffer_appendstr(&cmd, " -Werror -pedantic");
    }

    if (is_dylib(p)) {
        corto_buffer_appendstr(&cmd, " -dynamiclib");
    }

    /* LDFLAGS */
    bake_project_attr *flags_attr = p->get_attr("ldflags");
    if (flags_attr) {
        corto_iter it = corto_ll_iter(flags_attr->is.array);
        while (corto_iter_hasNext(&it)) {
            bake_project_attr *flag = corto_iter_next(&it);
            corto_buffer_append(&cmd, " %s", flag->is.string);
        }
    }

    corto_buffer_append(&cmd, " %s", source);

    if (corto_file_test(c->libpath)) {
        corto_buffer_append(&cmd, " -L%s", c->libpath);
    }

    if (strcmp(corto_getenv("BAKE_TARGET"), corto_getenv("BAKE_HOME"))) {
        corto_buffer_append(&cmd, " -L%s/lib", c->homepath);
    }

    corto_iter it = corto_ll_iter(p->link);
    while (corto_iter_hasNext(&it)) {
        char *dep = corto_iter_next(&it);
        corto_buffer_append(&cmd, " -l%s", dep);
    }

    bake_project_attr *libpath_attr = p->get_attr("libpath");
    if (libpath_attr) {
        corto_iter it = corto_ll_iter(libpath_attr->is.array);
        while (corto_iter_hasNext(&it)) {
            bake_project_attr *lib = corto_iter_next(&it);
            corto_buffer_append(&cmd, " -L%s", lib->is.string);

            if (is_darwin()) {
                corto_buffer_append(
                    &cmd, " -Xlinker -rpath -Xlinker %s", lib->is.string);
            }
        }
    }

    bake_project_attr *lib_attr = p->get_attr("lib");
    if (lib_attr) {
        corto_iter it = corto_ll_iter(lib_attr->is.array);
        while (corto_iter_hasNext(&it)) {
            bake_project_attr *lib = corto_iter_next(&it);
            const char *mapped = lib_map(lib->is.string);
            if (mapped) {
                corto_buffer_append(&cmd, " -l%s", mapped);
            }
        }
    }

    bake_project_attr *static_lib_attr = p->get_attr("static_lib");
    if (static_lib_attr) {
        corto_iter it = corto_ll_iter(static_lib_attr->is.array);
        while (corto_iter_hasNext(&it)) {
            bake_project_attr *lib = corto_iter_next(&it);
            if (hide_symbols) {
                /* If hiding symbols and linking with static library, unpack
                 * library objects to temp directory. If the library would be
                 * linked as-is, symbols would be exported, even though
                 * fvisibility is set to hidden */
                char *static_lib = find_static_lib(p, c, lib->is.string);
                if (!static_lib) {
                    continue;
                }

                char *cwd = strdup(corto_cwd());
                char *obj_path = corto_asprintf(".bake_cache/obj_%s/%s-%s",
                    lib->is.string, CORTO_PLATFORM_STRING, c->id);
                char *unpack_cmd = corto_asprintf("ar x %s", static_lib);

                /* The ar command doesn't have an option to output files to a
                 * specific directory, so have to use chdir. This will be an
                 * issue for multithreaded builds. */
                corto_mkdir(obj_path);
                corto_chdir(obj_path);
                l->exec(unpack_cmd);
                free(unpack_cmd);
                free(static_lib);
                corto_chdir(cwd);
                free(cwd);
                corto_buffer_append(&cmd, " %s/*", obj_path);

                if (!static_object_paths) {
                    static_object_paths = corto_ll_new();
                }

                corto_ll_append(static_object_paths, obj_path);
            } else {
                corto_buffer_append(&cmd, " -l%s", lib);
            }
        }
    }

    corto_buffer_append(&cmd, " -o %s", target);

    char *cmdstr = corto_buffer_str(&cmd);
    l->exec(cmdstr);
    free(cmdstr);

    /* If static libraries were unpacked, cleanup temporary directories */
    it = corto_ll_iter(static_object_paths);
    while (corto_iter_hasNext(&it)) {
        char *obj_path = corto_iter_next(&it);
        //corto_rm(obj_path);
        free(obj_path);
    }
    if (static_object_paths) {
        corto_ll_free(static_object_paths);
    }
}

static
void link_static_binary(
    bake_language *l,
    bake_project *p,
    bake_config *c,
    char *source,
    char *target,
    void *ctx)
{
    corto_buffer cmd = CORTO_BUFFER_INIT;
    corto_buffer_append(&cmd, "ar rcs %s %s", target, source);
    char *cmdstr = corto_buffer_str(&cmd);
    l->exec(cmdstr);
    free(cmdstr);
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
    bool link_static = p->get_attr_bool("static_artefact");

    if (link_static) {
        link_static_binary(l, p, c, source, target, ctx);
    } else {
        link_dynamic_binary(l, p, c, source, target, ctx);
    }
}

static
void init(
    bake_project *p)
{
    if (p->managed) {
        p->add_build_dependency("driver/gen/c/project");
        p->add_build_dependency("driver/gen/c/interface");
        if (p->model) {
            p->add_build_dependency("driver/gen/c/type");
            p->add_build_dependency("driver/gen/c/api");
            p->add_build_dependency("driver/gen/c/cpp");
        }
    }
}

static
void clean(
    bake_language *l,
    bake_project *p)
{
    p->clean("include/_project.h");
    p->clean("include/_type.h");
    p->clean("include/_load.h");
    p->clean("include/_interface.h");
    p->clean("include/_api.h");
    p->clean("include/_cpp.h");
    p->clean("include/_binding.h");
    p->clean("include/.prefix");
}

static
int16_t setup_project(
    bake_language *l,
    const char *id,
    bake_project_kind kind)
{
    /* Get short project id */
    const char *short_id = get_short_name(id);

    /* Create directories */
    corto_mkdir("src");
    corto_mkdir("include");

    /* Create project.json */
    FILE *f = fopen("project.json", "w");
    fprintf(f,
        "{\n"
        "    \"id\":\"%s\",\n"
        "    \"type\":\"%s\"\n"
        "}\n",
        id,
        kind == BAKE_APPLICATION
            ? "executable"
            : "library"
    );
    fclose(f);

    /* Create main source file */
    char *source_filename = corto_asprintf("src/%s.c", short_id);
    f = fopen(source_filename, "w");
    if (kind != BAKE_PACKAGE) {
        fprintf(f,
            "#include <include/%s.h>\n"
            "\n"
            "int main(int argc, char *argv[]) {\n"
            "    return 0;\n"
            "}\n",
            short_id
        );
    } else {
        fprintf(f,
            "#include <include/%s.h>\n"
            "\n",
            short_id
        );
    }
    fclose(f);
    free(source_filename);

    /* Create upper-case id for defines in header file */
    char *id_upper = strdup(id);
    strupper(id_upper);
    char *ptr, ch;
    for (ptr = id_upper; (ch = *ptr); ptr ++) {
        if (ch == '/' || ch == '.') {
            ptr[0] = '_';
        }
    }

    /* Create main header file */
    char *header_filename = corto_asprintf("include/%s.h", short_id);
    f = fopen(header_filename, "w");
    fprintf(f,
        "#ifndef %s_H\n"
        "#define %s_H\n"
        "\n"
        "#ifdef __cplusplus\n"
        "extern \"C\" {\n"
        "#endif\n"
        "\n"
        "#ifdef __cplusplus\n"
        "}\n"
        "#endif\n"
        "\n"
        "#endif\n"
        "\n",
        id_upper,
        id_upper
    );

    return 0;
}

static
bool project_is_managed(bake_project *p) {
    return p->managed;
}

static
bool project_has_model_and_public_and_package(bake_project *p) {
    return p->model != NULL && p->public && p->kind == BAKE_PACKAGE;
}

/* -- Rules */
int bakemain(bake_language *l) {

    /* Because the c driver is used to bootstrap build the corto package, it cannot
     * link with corto itself. Therefore, the driver is compiled with the
     * platform sources, that need to be manually initialized here. New language
     * bindings don't have this bootstrap problem and can use the corto package */
    corto_platform_init("driver/bake/c");

    /* Create pattern that matches generated source files */
    l->pattern("gen-sources", ".bake_cache/gen//*.c|*.cpp|*.cxx");

    l->pattern("gen-sources-2", ".bake_cache/gen//*.c|*.cpp|*.cxx");

    /* Create pattern that matches files in generated binding API */
    l->pattern("api-sources", "c|cpp");

    /* Generate rule for dynamically generating source for definition file */
    l->rule("GENERATED-SOURCES", "$MODEL,project.json", l->target_pattern("$gen-sources"), gen_source);

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

    /* Callback that initializes projects with the right build dependencies */
    l->init(init);

    /* Callback that specifies files to clean */
    l->clean(clean);

    /* Callback for generating artefact name(s) */
    l->artefact(artefact_name);

    /* Callback for setting up a project */
    l->setup_project(setup_project);

    return 0;
}
