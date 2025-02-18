# The coverage linker flag is specific for clang.
if (CONFIG_COVERAGE_NATIVE_GCOV)
  # GCC-compatible coverage implementation.
  set_linker_property(PROPERTY coverage --coverage)
elseif(CONFIG_COVERAGE_NATIVE_SOURCE)
  # clang’s source-based code coverage implementation. 
  # It’s called “source-based” because it operates on AST and preprocessor 
  # information directly. This allows it to generate very precise coverage data.
  set_linker_property(PROPERTY coverage -fprofile-instr-generate -fcoverage-mapping)
endif()

# Extra warnings options for twister run
set_linker_property(PROPERTY ld_extra_warning_options ${LINKERFLAGPREFIX},--fatal-warnings)

# GNU ld and LLVM lld complains when used with llvm/clang:
#   error: section: init_array is not contiguous with other relro sections
#
# So do not create RELRO program header.
set_linker_property(APPEND PROPERTY cpp_base ${LINKERFLAGPREFIX},-z,norelro)
