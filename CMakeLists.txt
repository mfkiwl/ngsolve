if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "RelWithDebInfo" CACHE STRING INTERNAL)
endif(NOT CMAKE_BUILD_TYPE)

cmake_minimum_required(VERSION 3.16)

# Handle superbuild first
option( USE_SUPERBUILD   "Whether or not a superbuild should be invoked" ON)
option( USE_LAPACK       "build ngsolve with LAPACK" ON)
option( USE_CUDA         "enable CUDA GPU support")
option( USE_MKL          "enable MKL")
option( USE_HYPRE        "enable HYPRE support")
option( USE_MUMPS        "enable sparse direct solver MUMPS")
option( USE_PARDISO      "enable pardiso sparse direct solver")
option( USE_UMFPACK      "enable umfpack sparse direct solver" ON)
option( INTEL_MIC        "cross compile for intel xeon phi")
option( USE_VTUNE        "include vtune pause/resume numproc")
option( USE_CCACHE       "use ccache")
option( INSTALL_DEPENDENCIES "install dependencies like netgen or solver libs, useful for packaging" OFF )
option( ENABLE_UNIT_TESTS "Enable Catch unit tests")
option( BUILD_STUB_FILES "Build stub files for better autocompletion" ON)
option( FAST_COMPILE "Skip optimized function code for faster compilation times and reduced code size" OFF )

set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_modules")
set(NETGEN_DIR "" CACHE PATH "Path to Netgen, leave empty to build Netgen automatically")

include (${CMAKE_CURRENT_LIST_DIR}/cmake/generate_version_file.cmake)

if(APPLE)
  set(INSTALL_DIR_DEFAULT /Applications/Netgen.app)
else(APPLE)
  if(WIN32)
    set(INSTALL_DIR_DEFAULT "C:/netgen")
  else(WIN32)
    set(INSTALL_DIR_DEFAULT /opt/netgen)
  endif(WIN32)
endif(APPLE)

if(INSTALL_DIR)
  message(WARNING "INSTALL_DIR is deprecated, use CMAKE_INSTALL_PREFIX instead")
  set(INSTALL_DIR_DEFAULT ${INSTALL_DIR})
endif(INSTALL_DIR)

if (USE_SUPERBUILD)
  project (NGSuite)
  if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${INSTALL_DIR_DEFAULT}" CACHE PATH "Install directory" FORCE)
  endif()
  # execute the superbuild (this script will be invoked again without the
  # USE_SUPERBUILD option this time)
  option( BUILD_UMFPACK "Download and build UMFPACK automatically" ON )
  include (cmake/SuperBuild.cmake)
  include (cmake/package.cmake)
  include(CPack)
  return()                      # stop processing this file further
else()
  project(NGSolve)
  if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
    set(CMAKE_INSTALL_PREFIX "${INSTALL_DIR_DEFAULT}" CACHE PATH "Install directory" FORCE)
  endif()
endif()

set(CMAKE_CXX_STANDARD 17)
if(EMSCRIPTEN)
    set(CMAKE_CXX_VISIBILITY_PRESET hidden)
endif()
find_package(Netgen REQUIRED CONFIG HINTS ${CMAKE_INSTALL_PREFIX}/Contents/Resources)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(NGSOLVE_INSTALL_DIR_PYTHON  ${NETGEN_INSTALL_DIR_PYTHON})
set(NGSOLVE_INSTALL_DIR_BIN     ${NETGEN_INSTALL_DIR_BIN})
set(NGSOLVE_INSTALL_DIR_LIB     ${NETGEN_INSTALL_DIR_LIB})
set(NGSOLVE_INSTALL_DIR_INCLUDE ${NETGEN_INSTALL_DIR_INCLUDE})
set(NGSOLVE_INSTALL_DIR_RES     ${NETGEN_INSTALL_DIR_RES})
string(REPLACE "netgen" "ngsolve" NGSOLVE_INSTALL_DIR_CMAKE ${NETGEN_INSTALL_DIR_CMAKE})

#######################################################################

set(NGSOLVE_COMPILE_INCLUDE_DIRS "")
set(NGSOLVE_COMPILE_DEFINITIONS "")
set(NGSOLVE_COMPILE_DEFINITIONS_PRIVATE "")
set(NGSOLVE_COMPILE_OPTIONS "")
list(APPEND NGSOLVE_COMPILE_DEFINITIONS HAVE_NETGEN_SOURCES)

#######################################################################

include (CheckIncludeFiles)
check_include_files (dlfcn.h HAVE_DLFCN_H)
include (CheckCXXSymbolExists)
check_cxx_symbol_exists ( abi::__cxa_demangle "cxxabi.h" HAVE_CXA_DEMANGLE )
if(HAVE_DLFCN_H)
    list(APPEND NGSOLVE_COMPILE_DEFINITIONS HAVE_DLFCN_H)
endif()
if(HAVE_CXA_DEMANGLE)
    list(APPEND NGSOLVE_COMPILE_DEFINITIONS HAVE_CXA_DEMANGLE)
endif()

add_custom_target(generate_version_file
  ${CMAKE_COMMAND}
    -DBDIR=${CMAKE_CURRENT_BINARY_DIR}
    -P ${CMAKE_CURRENT_LIST_DIR}/cmake/generate_version_file.cmake
    )

#######################################################################
if(FAST_COMPILE)
  list(APPEND NGSOLVE_COMPILE_DEFINITIONS FASTCOMPILE)
endif(FAST_COMPILE)
#######################################################################
if(USE_CCACHE)
  find_program(CCACHE_FOUND NAMES ccache ccache.bat)
  if(CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ${CCACHE_FOUND})
  endif(CCACHE_FOUND)
endif(USE_CCACHE)

#######################################################################
if(INTEL_MIC)
    set(MKL_ARCH "mic")
    include(cmake/mic.cmake)
endif(INTEL_MIC)

#######################################################################
if(ENABLE_UNIT_TESTS)
  include(${CMAKE_CURRENT_LIST_DIR}/cmake/external_projects/catch.cmake)
endif(ENABLE_UNIT_TESTS)

#######################################################################
# append install paths of software in non-standard paths (e.g. openmpi, metis, intel mkl, ...)
# cmake -DUSE_MPI=ON -DCMAKE_PREFIX_PATH="/opt/openmpi165;/opt/metis51" ../
set(ADDITIONAL_PATHS "" CACHE PATH "List of paths to additional libraries in non-standard locations, separated by ';'")
if (ADDITIONAL_PATHS)
  set(CMAKE_PREFIX_PATH ${ADDITIONAL_PATHS})
endif (ADDITIONAL_PATHS)

#######################################################################
# use rpath
set(CMAKE_MACOSX_RPATH TRUE)
if(NOT BUILD_FOR_CONDA)
    set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
endif()
set(CMAKE_INSTALL_RPATH "${NETGEN_RPATH_TOKEN};${NETGEN_RPATH_TOKEN}/${NETGEN_RPATH}")

#######################################################################
set(ngs_install_dir EXPORT ngsolve-targets RUNTIME DESTINATION ${NGSOLVE_INSTALL_DIR_BIN} COMPONENT ngsolve LIBRARY DESTINATION ${NGSOLVE_INSTALL_DIR_LIB} COMPONENT ngsolve_devel ARCHIVE DESTINATION ${NGSOLVE_INSTALL_DIR_LIB} COMPONENT ngsolve_devel )
install(EXPORT ngsolve-targets DESTINATION ${NGSOLVE_INSTALL_DIR_CMAKE} COMPONENT ngsolve_devel)

list(APPEND NGSOLVE_COMPILE_DEFINITIONS USE_TIMEOFDAY)

include_directories (BEFORE
  ${PROJECT_BINARY_DIR}
  ${PROJECT_SOURCE_DIR}/include
  ${PROJECT_SOURCE_DIR}/ngstd
  ${PROJECT_SOURCE_DIR}/basiclinalg
  ${PROJECT_SOURCE_DIR}/fem
  ${PROJECT_SOURCE_DIR}/linalg
  ${PROJECT_SOURCE_DIR}/comp
  ${PROJECT_SOURCE_DIR}/bem
  ${PROJECT_SOURCE_DIR}/multigrid
  ${PROJECT_SOURCE_DIR}/parallel
  ${PROJECT_SOURCE_DIR}/solve
  ${PROJECT_SOURCE_DIR}/ngscuda
  )
include_directories (AFTER ${NETGEN_INCLUDE_DIRS})

add_library(netgen_libs INTERFACE)
install(TARGETS netgen_libs ${ngs_install_dir})
target_link_libraries(netgen_libs INTERFACE nglib ngcore)

file(RELATIVE_PATH BIN_TO_LIB_RELPATH ${NETGEN_BINARY_DIR} ${NETGEN_LIBRARY_DIR})
if(NETGEN_USE_GUI)
  if(NOT NGSOLVE_INSTALL_DIR_TCL)
      set(NGSOLVE_INSTALL_DIR_TCL ${NGSOLVE_INSTALL_DIR_BIN})
  endif()
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ngsolve.tcl
    ${CMAKE_CURRENT_BINARY_DIR}/ngsolve.tcl @ONLY)

  install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ngsolve.tcl DESTINATION ${NGSOLVE_INSTALL_DIR_TCL} COMPONENT ngsolve)
endif(NETGEN_USE_GUI)

string(TOLOWER "${CMAKE_BUILD_TYPE}" CMAKE_BUILD_TYPE_LOWER)
if(CMAKE_BUILD_TYPE_LOWER STREQUAL "debug")
    list(APPEND NGSOLVE_COMPILE_DEFINITIONS DEBUG Wall)
endif()

if(EMSCRIPTEN)
    set(NGS_LIB_TYPE OBJECT)
    set(NGSOLVE_LIB_TYPE OBJECT)
elseif(WIN32)
    add_definitions(-DNGS_EXPORTS)

    # build convenience (aka object) libraries in windows)
    set(NGS_LIB_TYPE OBJECT)
    set(NGSOLVE_LIB_TYPE SHARED)

    if(CMAKE_CXX_COMPILER_ID STREQUAL "Intel")
        # Intel Compiler
        list(APPEND NGSOLVE_COMPILE_OPTIONS /Od /MP $<$<COMPILE_LANGUAGE:CXX>:/Qstd=c++17> /Qopt-report-file:report.log)
    else()
        # MS Visual Compiler
        list(APPEND NGSOLVE_COMPILE_OPTIONS /std:c++17 /bigobj /wd4068)
    endif()

else(WIN32)
    set(NGS_LIB_TYPE SHARED)
    set(NGSOLVE_LIB_TYPE SHARED)
    list(APPEND NGSOLVE_COMPILE_OPTIONS $<$<COMPILE_LANGUAGE:CXX>:-std=c++17>)
endif(EMSCRIPTEN)
if(APPLE)
    list(APPEND NGSOLVE_COMPILE_DEFINITIONS MSG_NOSIGNAL=0)
endif(APPLE)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    list(APPEND NGSOLVE_COMPILE_OPTIONS $<$<COMPILE_LANGUAGE:CXX>:-Wno-undefined-var-template -Wno-vla-extension>)
endif()


#######################################################################
if (NETGEN_USE_GUI)
    list(APPEND NGSOLVE_COMPILE_DEFINITIONS TCL)
endif (NETGEN_USE_GUI)

add_library(ngs_lapack INTERFACE )
install( TARGETS ngs_lapack ${ngs_install_dir} )
if (USE_LAPACK)
    if(NOT LAPACK_LIBRARIES)
      find_package(LAPACK REQUIRED)
    endif()
    target_link_libraries(ngs_lapack INTERFACE "${LAPACK_LIBRARIES}")
    list(APPEND NGSOLVE_COMPILE_DEFINITIONS LAPACK)
endif (USE_LAPACK)

#######################################################################
set(MAX_SYS_DIM "3" CACHE STRING "Maximum compile-time size of SparseMatrix entries.")
if (NOT MAX_SYS_DIM)
   set(MAX_SYS_DIM "3")
endif (NOT MAX_SYS_DIM)
message(STATUS "MAX_SYS_DIM = ${MAX_SYS_DIM}")
list (APPEND NGSOLVE_COMPILE_OPTIONS -DMAX_SYS_DIM=${MAX_SYS_DIM})

#######################################################################
if (USE_MKL)
    set(NG_USE_MPI ${USE_MPI})
    unset(USE_MPI CACHE)
    find_package(MKL REQUIRED)
    set(USE_MPI ${NG_USE_MPI} CACHE STRING "enable mpi parallelization")
    target_include_directories(ngs_lapack INTERFACE "${MKL_INCLUDE_DIRS}")
    list(APPEND NGSOLVE_COMPILE_DEFINITIONS USE_PARDISO)
    list(APPEND NGSOLVE_COMPILE_DEFINITIONS_PRIVATE USE_MKL)

    if("${MKL_INTERFACE_LAYER}" STREQUAL "_ilp64" OR ${MKL_INTERFACE} STREQUAL "ilp64")
      list(APPEND NGSOLVE_COMPILE_DEFINITIONS MKL_ILP64)
    endif()
    if(MKL_STATIC AND WIN32)
      install(FILES ${MKL_RTL_LIBRARY_BIN} DESTINATION ${NGSOLVE_INSTALL_DIR_BIN} COMPONENT ngsolve )
    endif()
endif (USE_MKL)

if(MKL_STATIC)
  set(LAPACK_CMAKE_LINK_INTERFACE PRIVATE)
else()
  set(LAPACK_CMAKE_LINK_INTERFACE PUBLIC)
endif()

#######################################################################
if (USE_HYPRE)
  if (NOT NETGEN_USE_MPI)
      message(FATAL_ERROR "Hypre needs MPI to be enabled, but Netgen was configured without MPI!")
  endif (NOT NETGEN_USE_MPI)
  find_package(HYPRE REQUIRED)
  list(APPEND NGSOLVE_COMPILE_DEFINITIONS HYPRE)
  include_directories(${HYPRE_INCLUDES})
endif(USE_HYPRE)

if (USE_MUMPS)
    enable_language(Fortran)
    find_package(MUMPS REQUIRED)
    find_package(MPI REQUIRED)
    list(APPEND NGSOLVE_COMPILE_DEFINITIONS USE_MUMPS)
    include_directories(${MUMPS_INCLUDE_DIR})
    link_directories(${MUMPS_DIR}/lib)
endif (USE_MUMPS)

#######################################################################
add_library(netgen_python INTERFACE )
if (NETGEN_USE_PYTHON)
    list(APPEND NGSOLVE_COMPILE_DEFINITIONS NGS_PYTHON)
    if(BUILD_FOR_CONDA AND NOT WIN32)
        find_package(PythonInterp 3 REQUIRED)
        if(NOT BUILD_FOR_CONDA)
          find_package(PythonLibs 3 REQUIRED)
        endif()
        target_include_directories(netgen_python INTERFACE ${PYTHON_INCLUDE_DIR})
        if(APPLE)
          set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -undefined dynamic_lookup")
        endif()
    else()
        target_link_libraries(netgen_python INTERFACE ${NETGEN_PYTHON_LIBRARIES})
        target_include_directories(netgen_python INTERFACE ${NETGEN_PYBIND_INCLUDE_DIR} ${NETGEN_PYTHON_INCLUDE_DIRS})
    endif()
endif (NETGEN_USE_PYTHON)

#######################################################################

if(USE_PARDISO)
  list(APPEND NGSOLVE_COMPILE_DEFINITIONS USE_PARDISO)
  list(APPEND NGSOLVE_COMPILE_DEFINITIONS USE_PARDISO400)
  if(NOT PARDISO_LIB)
      find_library(PARDISO_LIB NAMES libpardiso500-GNU481-X86-64.so)
  endif(NOT PARDISO_LIB)
  if(NOT PARDISO_LIB)
      message(FATAL_ERROR "Could NOT find Pardiso")
  endif(NOT PARDISO_LIB)
  find_package(OpenMP)
  set(PARDISO_LIBS ${PARDISO_LIB} ${OpenMP_gomp_LIBRARY} ${OpenMP_pthread_LIBRARY})
endif(USE_PARDISO)

#######################################################################

if(USE_UMFPACK)
  set(SuiteSparse_USE_LAPACK_BLAS ON)
  find_package(suitesparse QUIET NO_MODULE HINTS ${UMFPACK_DIR})
  if(suitesparse_FOUND)
    set(UMFPACK_LIBRARIES SuiteSparse::umfpack)
  else()
    # UMFPACK was not built with Superbuild, look for it manually
    find_library(UMFPACK_LIB REQUIRED NAMES umfpack HINTS ${UMFPACK_DIR}/lib ${UMFPACK_DIR}/lib64)
    find_library(CHOLMOD_LIB REQUIRED NAMES cholmod HINTS ${UMFPACK_DIR}/lib ${UMFPACK_DIR}/lib64)
    find_library(AMD_LIB REQUIRED NAMES amd HINTS ${UMFPACK_DIR}/lib ${UMFPACK_DIR}/lib64)
    find_library(CCOLAMD_LIB REQUIRED NAMES ccolamd HINTS ${UMFPACK_DIR}/lib ${UMFPACK_DIR}/lib64)
    find_library(SUITESPARSECONFIG_LIB REQUIRED NAMES suitesparseconfig HINTS ${UMFPACK_DIR}/lib ${UMFPACK_DIR}/lib64)
    set(UMFPACK_LIBRARIES ${UMFPACK_LIB} ${CHOLMOD_LIB} ${AMD_LIB} ${CCOLAMD_LIB} ${SUITESPARSECONFIG_LIB})
    find_path(UMFPACK_INCLUDE_DIR REQUIRED NAMES umfpack.h PATH_SUFFIXES suitesparse HINTS ${UMFPACK_DIR}/include ${UMFPACK_DIR}/include/suitesparse)
  endif()
  list(APPEND NGSOLVE_COMPILE_DEFINITIONS USE_UMFPACK)
endif(USE_UMFPACK)


#######################################################################
if(USE_VTUNE)
    list(APPEND NGSOLVE_COMPILE_DEFINITIONS VTUNE)
    find_path(VTUNE_INCLUDE_DIR libittnotify.h)
    include_directories(${VTUNE_INCLUDE_DIR})
    get_filename_component(VTUNE_DIR ${VTUNE_INCLUDE_DIR} PATH)
    if(INTEL_MIC)
        find_library(VTUNE_LIB libittnotify.a PATH ${VTUNE_DIR}/bin64/k1om)
    else(INTEL_MIC)
        find_library(VTUNE_LIB libittnotify.a PATH ${VTUNE_DIR}/lib64)
    endif(INTEL_MIC)
endif(USE_VTUNE)

#######################################################################
if(${CMAKE_BUILD_TYPE} STREQUAL "Debug" OR ${CMAKE_BUILD_TYPE} STREQUAL "DEBUG")
  set(NGS_TEST_TIMEOUT 300)
else()
  set(NGS_TEST_TIMEOUT 60)
endif()

if(NETGEN_USE_PYTHON AND NOT SKBUILD)
  # install egg file to let python/pip know that Netgen ist installed
  file( WRITE "ngsolve-py3.egg-info"
"Metadata-Version: 2.1
Name: ngsolve
Version: ${NGSOLVE_VERSION_MAJOR}.${NGSOLVE_VERSION_MINOR}.${NGSOLVE_VERSION_PATCH}.post${NGSOLVE_VERSION_TWEAK}
")
  install(FILES ngsolve-py3.egg-info DESTINATION ${NGSOLVE_INSTALL_DIR_PYTHON} COMPONENT ngsolve)
endif()

#######################################################################
# generate ngscxx compiler wrapper

# compile flags and preprocessor definitions from ngcore (skip options if none are set)
get_target_property(ngcore_compile_definitions ngcore INTERFACE_COMPILE_DEFINITIONS)
get_property(have_options TARGET ngcore PROPERTY INTERFACE_COMPILE_OPTIONS SET)
if(have_options)
    get_target_property(ngcore_compile_options ngcore INTERFACE_COMPILE_OPTIONS)
endif(have_options)
set(ngscxx_compile_definitions ${NGSOLVE_COMPILE_DEFINITIONS} ${ngcore_compile_definitions})

foreach( d ${ngscxx_compile_definitions} )
    if(WIN32)
      if(NOT ${d} STREQUAL "NGS_EXPORTS")
        set( ngscxx_define_flags "${ngscxx_define_flags} /D${d}" )
      endif()
    else()
      set( ngscxx_define_flags "${ngscxx_define_flags} -D${d}" )
    endif()
endforeach()

string(TOUPPER "CMAKE_CXX_FLAGS_${CMAKE_BUILD_TYPE}" build_type_name)
set(ngscxx_flags "${${build_type_name}} ${CMAKE_CXX_FLAGS} ${ngscxx_define_flags}")

if(WIN32)
  string(REPLACE "/Zi " " " ngscxx_flags ${ngscxx_flags})
endif(WIN32)


foreach( opt ${NGSOLVE_COMPILE_OPTIONS} ${ngcore_compile_options} )
    set(ngscxx_flags "${ngscxx_flags} ${opt}")
endforeach()
if(NOT WIN32)
  set(ngscxx_flags "${ngscxx_flags} -fPIC")
endif()

# include directories
set(ngscxx_include_dirs ${NETGEN_MPI_CXX_INCLUDE_PATH} ${MKL_INCLUDE_DIRS} ${MUMPS_INCLUDE_DIR} ${NETGEN_PYTHON_INCLUDE_DIRS} ${UMFPACK_INCLUDE_DIR} ${VTUNE_INCLUDE_DIR})


foreach( d ${ngscxx_include_dirs} )
  if(WIN32)
    set( ngscxx_includes "${ngscxx_includes} /I\"${d}\"" )
  else()
    set( ngscxx_includes "${ngscxx_includes} -I${d}" )
  endif()
endforeach()

file(RELATIVE_PATH BIN_TO_INC_RELPATH ${NETGEN_BINARY_DIR} ${NETGEN_INCLUDE_DIR})
if(WIN32)
  get_filename_component(VC_BIN_DIR ${CMAKE_CXX_COMPILER} PATH)
  find_file(VCVARSALL vcvarsall.bat PATHS ${VC_BIN_DIR}/../../ ${VC_BIN_DIR}/../../../../../../Auxiliary/Build/)
  set( ngscxx_set_script_dir "\
set NGSCXX_DIR=%~dp0
call \"${VCVARSALL}\" amd64
" )
  set( ngscxx_includes "${ngscxx_includes} /I\"%NGSCXX_DIR%/${BIN_TO_INC_RELPATH}\"" )
  set( ngscxx_includes "${ngscxx_includes} /I\"%NGSCXX_DIR%/${BIN_TO_INC_RELPATH}/include\"" )
else()
  set( ngscxx_set_script_dir "\
NGSCXX_DIR=$( cd $(dirname $0); pwd )
PY_INCLUDE_FLAGS=-I$(python3 -c\"import sysconfig;print(sysconfig.get_path('include'))\")
if [ -f ${CMAKE_CXX_COMPILER} ]
  then NGSCXX=${CMAKE_CXX_COMPILER}
  else NGSCXX=c++
fi
export CCACHE_NOHASHDIR=1
")
  set( ngscxx_includes "${ngscxx_includes} -I\$NGSCXX_DIR/${BIN_TO_INC_RELPATH}" )
  set( ngscxx_includes "${ngscxx_includes} -I\$NGSCXX_DIR/${BIN_TO_INC_RELPATH}/include" )
  set( ngscxx_includes "${ngscxx_includes} \$PY_INCLUDE_FLAGS" )
  find_program(BASH bash REQUIRED)
endif()

# linker flags
if(USE_MKL AND NOT INSTALL_DEPENDENCIES AND NOT MKL_STATIC AND WIN32)
    set( ngsld_flags "${ngsld_flags} ${MKL_LINK_FLAGS}" )
endif()

if(USE_MPI)
  set( ngsld_flags "${ngsld_flags} ${MPI_LINK_FLAGS}" )
  foreach( lib ${MPI_CXX_LIBRARIES} )
    set(ngsld_flags "${ngsld_flags} ${lib}")
  endforeach()
endif(USE_MPI)

if(APPLE)
    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/pictures/logo.icns DESTINATION ${NGSOLVE_INSTALL_DIR_RES}/../ RENAME Netgen.icns)
    if(CMAKE_OSX_SYSROOT)
        set( ngscxx_includes "-isysroot ${CMAKE_OSX_SYSROOT} ${ngscxx_includes}" )
        set( ngsld_flags "-isysroot ${CMAKE_OSX_SYSROOT} ${ngsld_flags}" )
    endif()
    #   set(ngscxx_flags "${ngscxx_flags} -Qunused-arguments")
    set(ngscxx_includes "${ngscxx_includes} -I$Netgen_BUNDLE/Contents/Resources/include" )  
    set(ngsld_flags "${ngsld_flags} -L$Netgen_BUNDLE/Contents/MacOS  -undefined dynamic_lookup" )
    set(ngscxx_header "\
#!${BASH}
${ngscxx_set_script_dir}
Netgen_BUNDLE=\"`echo \"$0\" | sed -e 's/\\/Contents\\/MacOS\\/ngscxx//'`\"
Netgen_MACOS=\"$Netgen_BUNDLE/Contents/MacOS\"
")
    set(ngsld_header "\
#!${BASH}
${ngscxx_set_script_dir}
Netgen_BUNDLE=\"`echo \"$0\" | sed -e 's/\\/Contents\\/MacOS\\/ngsld//'`\"
Netgen_MACOS=\"$Netgen_BUNDLE/Contents/MacOS\"
")
file(RELATIVE_PATH BIN_TO_LIB_RELPATH ${NETGEN_BINARY_DIR} ${NETGEN_LIBRARY_DIR})
elseif(WIN32)
    set(ngsld_flags "${ngsld_flags} /LIBPATH:\"%NGSCXX_DIR%/${BIN_TO_LIB_RELPATH}\" nglib.lib ngcore.lib libngsolve.lib" )
    set(ngsld_header
"set PYTHON_LIBRARY=${NETGEN_PYTHON_LIBRARIES}
")
    if(NETGEN_USE_PYTHON)
      set(ngsld_header
"for /f  %%a in ('python -c \"import sys,os; print(os.path.join(sys.base_prefix, 'libs'))\"') do set PYTHON_LIBDIR=\"%%a\"
")
      set(ngsld_flags "${ngsld_flags} /LIBPATH:\"%PYTHON_LIBDIR%\"")
    endif()
else()
    set(ngsld_flags "${ngsld_flags} -L\$NGSCXX_DIR/${BIN_TO_LIB_RELPATH} -Wl,--rpath=\$NGSCXX_DIR/${BIN_TO_LIB_RELPATH}" )
    set(ngscxx_header
"#!${BASH}
${ngscxx_set_script_dir}
")
    set(ngsld_header
"#!${BASH}
${ngscxx_set_script_dir}
")
endif()

if(WIN32)
    set(NGSCXX_COMMAND ngscxx.bat)
    file(GENERATE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/ngscxx.bat
                  CONTENT "${ngscxx_set_script_dir}\n cl /c ${ngscxx_flags} ${ngscxx_includes} %*\n"
                  CONDITION $<CONFIG:${CMAKE_BUILD_TYPE}>
                  )
    file(GENERATE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/ngsld.bat
                  CONTENT "${ngscxx_set_script_dir}\n ${ngsld_header}\nlink /DLL %* ${ngsld_flags}\n")
    install (PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/ngscxx.bat DESTINATION ${NGSOLVE_INSTALL_DIR_BIN} COMPONENT ngsolve_devel )
    install (PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/ngsld.bat DESTINATION ${NGSOLVE_INSTALL_DIR_BIN} COMPONENT ngsolve_devel )
else(WIN32)
    set(NGSCXX_COMMAND ngscxx)
    set(ngscxx_compile_command "$NGSCXX ${ngscxx_flags} ${ngscxx_includes} $*")
    if(CCACHE_FOUND)
      set(ngscxx_compile_command "
if [ -f ${CCACHE_FOUND} ]
  then ${CCACHE_FOUND} ${ngscxx_compile_command}
  else ${ngscxx_compile_command}
fi")
    endif(CCACHE_FOUND)
    file(GENERATE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/ngscxx
                  CONTENT "${ngscxx_header}\n${ngscxx_compile_command}\n"
                  CONDITION $<COMPILE_LANGUAGE:CXX>
                  )
    file(GENERATE OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/ngsld CONTENT "${ngsld_header}\n$NGSCXX ${ngsld_flags} $*\n")
    install (PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/ngscxx DESTINATION ${NGSOLVE_INSTALL_DIR_BIN} COMPONENT ngsolve_devel )
    install (PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/ngsld DESTINATION ${NGSOLVE_INSTALL_DIR_BIN} COMPONENT ngsolve_devel )
endif(WIN32)

if (NETGEN_USE_MPI)
    # generate python3 wrapper for mpi (to preload mkl and mpi libs)
    set(ngspy_preload_libs "${MPI_C_LIBRARIES}")
    if(USE_MKL AND NOT MKL_STATIC AND NOT MKL_SDL)
      find_library(GOMP_LIBRARY NAMES libgomp.so.1 REQUIRED HINTS "/usr/lib/x86_64-linux-gnu/")
      set(ngspy_preload_libs "${ngspy_preload_libs}:${MKL_CORE_LIBRARY}:${MKL_THREADING_LIBRARY}:${MKL_INTERFACE_LIBRARY}:${MKL_BLACS_LIBRARY}:${GOMP_LIBRARY}")
    endif()
    find_program(BASH bash)
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/ngspy
"#! ${BASH}
LD_PRELOAD=$LD_PRELOAD:${ngspy_preload_libs} ${NETGEN_PYTHON_EXECUTABLE} $*
")
    install (PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/ngspy DESTINATION ${NGSOLVE_INSTALL_DIR_BIN} COMPONENT ngsolve_devel )
endif (NETGEN_USE_MPI)


#######################################################################

add_subdirectory(cmake/resources)
add_subdirectory(include)
add_subdirectory(ngstd)
add_subdirectory(basiclinalg)
add_subdirectory(parallel)
add_subdirectory(linalg)
add_subdirectory(fem)
add_subdirectory(bem)
add_subdirectory(multigrid)
add_subdirectory(comp)
add_subdirectory(solve)
add_subdirectory(python)
add_subdirectory(pde_tutorial)
if(NETGEN_USE_PYTHON)
  add_subdirectory(py_tutorials)
endif(NETGEN_USE_PYTHON)
add_subdirectory(tests)
add_subdirectory(docs)
if(USE_CUDA)
    add_subdirectory(ngscuda)
endif(USE_CUDA)
add_subdirectory(emscripten)



#######################################################################
# CTest
enable_testing()
include(CTest)
add_custom_target(performance_tests ctest -L performance -D Experimental)


#######################################################################
# uninstall target
configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/cmake_uninstall.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake"
    IMMEDIATE @ONLY)
# add_custom_target(uninstall
#     COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_BINARY_DIR}/cmake_uninstall.cmake)

get_filename_component(NGS_CMAKE_DIR_ABS ${NGSOLVE_INSTALL_DIR_CMAKE} ABSOLUTE BASE_DIR ${CMAKE_INSTALL_PREFIX})
file(RELATIVE_PATH CMAKE_TO_PREFIX_RELPATH ${NGS_CMAKE_DIR_ABS} ${CMAKE_INSTALL_PREFIX})
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/cmake/NGSolveConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/NGSolveConfig.cmake @ONLY ESCAPE_QUOTES)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/NGSolveConfig.cmake DESTINATION ${NGSOLVE_INSTALL_DIR_CMAKE} COMPONENT ngsolve_devel)


#######################################################################
# Configure message
# TODO: other message in case of failure
set(flags "${ngscxx_flags}")
message("
------------------------------------------------------------------------
  ${PROJECT_NAME} ${NGSOLVE_VERSION}:  Automatic configuration OK.

  Build type: ${CMAKE_BUILD_TYPE}
  Compiler: ${CMAKE_CXX_COMPILER}
  Flags: ${flags}

  Enabled functionality:

    GUI: ............... ${NETGEN_USE_GUI}
    LAPACK: ............ ${USE_LAPACK}
    MKL: ............... ${USE_MKL}
    PYTHON: ............ ${NETGEN_USE_PYTHON}
    MPI: ............... ${NETGEN_USE_MPI}
    CUDA: .............. ${USE_CUDA}
    MUMPS: ............. ${USE_MUMPS}
    PARDISO: ........... ${USE_PARDISO}
    INTEL_MIC: ......... ${INTEL_MIC}
    VTUNE: ............. ${USE_VTUNE}


  Building:
  ")
if(WIN32)
message("
    Open ${CMAKE_BINARY_DIR}/NGSolve.sln and build solution to compile ${PROJECT_NAME}.

    Build \"INSTALL\" to install ${PROJECT_NAME}.
")
else(WIN32)
message("
    Type 'make' to compile ${PROJECT_NAME}.

    Type 'make install' to install ${PROJECT_NAME}.

")
endif(WIN32)
message("
  Install directory:

    ${CMAKE_INSTALL_PREFIX}")
message("
------------------------------------------------------------------------
")

if (ADDITIONAL_PATHS)
  message(WARNING "The use of ADDITIONAL_PATHS is deprecated, use CMAKE_PREFIX_PATH instead.")
endif (ADDITIONAL_PATHS)


