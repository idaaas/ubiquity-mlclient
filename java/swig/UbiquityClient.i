%module Ubiquity

%include <stdint.i>
%include <std_string.i>

%javaconst(0);

%{
#include "Ubiquity.hpp"
%}

%include "Ubiquity.hpp"

%extend Ubiquity {
#if defined(SWIG)
%proxycode %{
  static {
    System.out.println("[INFO] UbiquityClient: trying to load native lib");

    try {
      ai.ubiquity.NativeLoader.load();
    } catch(Exception e) {
      e.printStackTrace();
      throw new RuntimeException("Cannot load native lib");
    }

    System.out.println("[INFO] UbiquityClient: native lib loaded");
  }
%}
#endif
}
