// GAU wasm node function (C++, no exceptions/RTTI, no stdlib).
// Formats a Vector3f (three float input pins) into "{x, y, z}" on a single
// string output pin. Freestanding: all formatting is done by hand.
// @node category=Pure
// @in x:float y:float z:float
// @out result:string
#include "gau_api.h"

// Writes a non-negative integer as decimal digits. Returns the char count.
static int WriteInt(char* out, long value)
{
    if (value == 0) {
        out[0] = '0';
        return 1;
    }
    char tmp[20];
    int t = 0;
    while (value > 0) {
        tmp[t++] = static_cast<char>('0' + (value % 10));
        value /= 10;
    }
    for (int i = 0; i < t; ++i) {
        out[i] = tmp[t - 1 - i];
    }
    return t;
}

// Writes a float with two fractional digits. Returns the char count.
static int WriteFloat(char* out, float f)
{
    int n = 0;
    if (f < 0.0f) {
        out[n++] = '-';
        f = -f;
    }
    long ip = static_cast<long>(f);
    long fd = static_cast<long>((f - static_cast<float>(ip)) * 100.0f + 0.5f);
    if (fd >= 100) { // rounding carried into the integer part
        fd -= 100;
        ip += 1;
    }
    n += WriteInt(out + n, ip);
    out[n++] = '.';
    out[n++] = static_cast<char>('0' + (fd / 10) % 10);
    out[n++] = static_cast<char>('0' + fd % 10);
    return n;
}

extern "C" void MakeStringFromVector3f(void)
{
    const Vector3f v = gau_read_Vector3f(0);
    char buf[128];
    int n = 0;
    buf[n++] = '{';
    n += WriteFloat(buf + n, v.x);
    buf[n++] = ',';
    buf[n++] = ' ';
    n += WriteFloat(buf + n, v.y);
    buf[n++] = ',';
    buf[n++] = ' ';
    n += WriteFloat(buf + n, v.z);
    buf[n++] = '}';
    gau_output_str(0, buf, n);
}
