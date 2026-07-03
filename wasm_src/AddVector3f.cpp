// GAU wasm node function (C++, no exceptions/RTTI, no stdlib).
// @node category=Pure
// @in ax:float ay:float az:float bx:float by:float bz:float
// @out x:float y:float z:float
#include "gau_api.h"

extern "C" void AddVector3f(void)
{
    const Vector3f a = gau_read_Vector3f(0);
    const Vector3f b = gau_read_Vector3f(3);
    gau_write_Vector3f(0, Vector3f{a.x + b.x, a.y + b.y, a.z + b.z});
}
