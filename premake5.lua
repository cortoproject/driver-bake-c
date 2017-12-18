
workspace "c"
  configurations { "debug", "release" }
  location "build"

  configuration { "linux", "gmake" }
    buildoptions { "-std=c99", "-D_XOPEN_SOURCE=600" }

  project "c"
    kind "SharedLib"
    language "C"
    location "build"
    targetdir "."

    objdir ".bake_cache"

    files { "include/*.h", "src/*.c", "../base/src/*.c" }
    includedirs { ".", "../base", "../builder", "$(BAKE_HOME)/include/corto/$(BAKE_VERSION)" }

    configuration "linux"
      links { "rt", "dl", "m", "ffi", "pthread" }

    configuration "macosx"
      links { "dl", "m", "ffi", "pthread" }

    configuration "debug"
      defines { "DEBUG" }
      symbols "On"

    configuration "release"
      defines { "NDEBUG" }
      optimize "On"
