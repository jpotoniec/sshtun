#ifndef UTILS_HPP
#define UTILS_HPP

inline void sensibleCopy(char *dst, const char *src, size_t n)
{
    strncpy(dst, src, n);
    dst[n-1]='\0';
}

#endif // UTILS_HPP
