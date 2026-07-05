// GAU wasm node function (C++, no exceptions/RTTI, no stdlib).
// Formats a Vector3f (three float input pins) into "x, y, z" on a string
// output pin using the GauStr helpers from gau_api.h. Standard headers
// (<iostream>, <string>, ...) are unavailable: wasm functions build
// freestanding (-nostdlib).
// @node category=Pure
// @in x:float y:float z:float
// @out result:string
#include "gau_api.h"

extern "C" void my_function(void)
{
    const Vector3f v = gau_read_Vector3f(0);
    GauStr s = gau_str();
    gau_append(s, v.x);
    gau_append(s, ", ");
    gau_append(s, v.y);
    gau_append(s, ", ");
    gau_append(s, v.z);
    gau_output_str(0, s);
}
