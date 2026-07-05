// GAU wasm node function (C++, no exceptions/RTTI, no stdlib).
// The directives below define the node class (created/updated on
// build). Pin tokens are name:type; use _ for unnamed exec pins.
// @node category=Function
// @in a:int b:int
// @out result:int

// gau_api.h is generated from the node classes: host imports plus
// structs and gau_read_/gau_write_ helpers for data classes.
#include "gau_api.h"

extern "C" void my_function(void)
{
    gau_output_i32(0, gau_input_i32(0) + gau_input_i32(1));
}
