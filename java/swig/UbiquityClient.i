%module Ubiquity

%include <stdint.i>

/* Force the generated Java code to use the C constant values rather than making a JNI call */
%javaconst(1);

%{
#include "Ubiquity.hpp"
%}

%include "Ubiquity.hpp"
