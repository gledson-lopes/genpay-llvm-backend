add_llvm_component_library(LLVMGenpayAsmParser
  GenpayAsmParser.cpp

  LINK_COMPONENTS
  MC
  MCParser
  Support
  GenpayDesc
  GenpayInfo

  DEPENDS
  GenpayCommonTableGen
  intrinsics_gen
  )
