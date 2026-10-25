add_llvm_component_library(LLVMGenpayDesc
  GenpayAsmBackend.cpp
  GenpayELFObjectWriter.cpp
  GenpayInstPrinter.cpp
  GenpayMCCodeEmitter.cpp
  GenpayMCInstLower.cpp
  GenpayMCTargetDesc.cpp

  DEPENDS
  GenpayCommonTableGen

  LINK_COMPONENTS
  MC
  Support
  GenpayInfo
  )
