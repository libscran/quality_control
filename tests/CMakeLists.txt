include(FetchContent)
FetchContent_Declare(
  scran_tests 
  GIT_REPOSITORY https://github.com/libscran/scran_tests
  GIT_TAG master
)
FetchContent_MakeAvailable(scran_tests)

option(CODE_COVERAGE "Enable coverage testing" OFF)
set(DO_CODE_COVERAGE OFF)
if(CODE_COVERAGE AND CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    set(DO_CODE_COVERAGE ON)
endif()

include(GoogleTest)

macro(create_test name) 
    add_executable(
        ${name} 
        src/per_cell_qc_metrics.cpp
        src/find_median_mad.cpp
        src/choose_filter_thresholds.cpp
        src/rna_quality_control.cpp
        src/adt_quality_control.cpp
        src/crispr_quality_control.cpp
        src/format_filters.cpp
    )

    target_link_libraries(
        ${name}
        scran_qc
        scran_tests
    )

    target_compile_options(${name} PRIVATE -Wall -Werror -Wpedantic -Wextra)

    if(DO_CODE_COVERAGE)
        target_compile_options(${name} PRIVATE -O0 -g --coverage)
        target_link_options(${name} PRIVATE --coverage)
    endif()

    gtest_discover_tests(${name})
endmacro()

create_test(libtest)

find_package(OpenMP)
if (OpenMP_FOUND)
    create_test(omptest)
    target_link_libraries(omptest OpenMP::OpenMP_CXX)
endif()
