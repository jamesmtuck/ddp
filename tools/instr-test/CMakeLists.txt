# get llvm libs

if ("${LLVM_PACKAGE_VERSION}" VERSION_GREATER "3.4.2")
  add_executable(instr-test main.cpp)
  llvm_map_components_to_libnames(llvm_libs analysis bitreader bitwriter codegen core ipa asmparser irreader instcombine instrumentation mc objcarcopts scalaropts support ipo target transformutils vectorize)
else()
  add_executable(instr-test main.cpp)
  llvm_map_components_to_libraries(llvm_libs bitreader bitwriter asmparser irreader instrumentation scalaropts ipo vectorize)
endif()

#add library dependences on instr-test
target_link_libraries(instr-test instrument)
target_link_libraries(instr-test ${llvm_libs})

install(TARGETS instr-test
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION lib
        ARCHIVE DESTINATION lib)
