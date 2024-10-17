%module Ubiquity

/* Go does not have class hierarchy concept */
%feature ("flatnested");

%include <stdint.i>

%{
#include "Ubiquity.hpp"
%}

%include "Ubiquity.hpp"
