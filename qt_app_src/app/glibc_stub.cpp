// 兼容补丁: gcc-11 libstdc++ 引用 glibc>=2.32 才有的 __libc_single_threaded,
// 而板子 buster 的 glibc 2.28 没有该符号。这里提供一份本地定义(buster 不导出, 无冲突)。
extern "C" const char __libc_single_threaded = 1;
