# get llvm libs

if ("${LLVM_PACKAGE_VERSION}" VERSION_GREATER "3.4.2")
  add_executable(ddp main.cpp PassPrinters.cpp)
  llvm_map_components_to_libnames(llvm_libs analysis bitreader bitwriter codegen core ipa asmparser irreader instcombine instrumentation mc objcarcopts scalaropts support ipo target transformutils vectorize)
else()
  add_executable(ddp main.cpp)
  llvm_map_components_to_libraries(llvm_libs bitreader bitwriter asmparser irreader instrumentation scalaropts ipo vectorize)
endif()

#add library dependences on ddp
target_link_libraries(ddp setprof edgeprof addrprof profdb instrument)
target_link_libraries(ddp ${llvm_libs})
target_link_libraries(ddp sqlite3)

install(TARGETS ddp 
        RUNTIME DESTINATION bin)
