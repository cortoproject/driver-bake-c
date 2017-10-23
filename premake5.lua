
workspace "c"
  configurations { "Debug", "Release" }
  location "build"

  configuration { "linux", "gmake" }
    buildoptions { "-std=c99", "-D_XOPEN_SOURCE=600" }

  project "c"
    kind "SharedLib"
    language "C"
    location "build"
    targetdir "."

    files { "include/*.h", "src/*.c", "../base/src/*.c" }
    includedirs { ".", "../base", "../builder", "$(CORTO_HOME)/include/corto/$(CORTO_VERSION)" }

    if os.is64bit then
      objdir (".corto/obj/" .. os.target() .. "-64")
    else
      objdir (".corto/obj/" .. os.target() .. "-32")
    end

    configuration "linux"
      links { "rt", "dl", "m", "ffi", "pthread" }    

    configuration "Debug"
      defines { "DEBUG" }
      symbols "On"

    configuration "Release"
      defines { "NDEBUG" }
      optimize "On"

