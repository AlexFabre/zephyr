# The coverage linker flag is specific for clang.
if (CONFIG_COVERAGE_NATIVE_GCOV)
  set_linker_property(PROPERTY coverage --coverage)
elseif(CONFIG_COVERAGE_NATIVE_SOURCE)
  set_linker_property(PROPERTY coverage -fprofile-instr-generate -fcoverage-mapping)
endif()

# Extra warnings options for twister run
set_linker_property(PROPERTY ld_extra_warning_options ${LINKERFLAGPREFIX},--fatal-warnings)

# GNU ld and LLVM lld complains when used with llvm/clang:
#   error: section: init_array is not contiguous with other relro sections
#
# So do not create RELRO program header.
set_linker_property(APPEND PROPERTY cpp_base ${LINKERFLAGPREFIX},-z,norelro)
