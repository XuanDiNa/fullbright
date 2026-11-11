#pragma once
#include <cstdio>
inline unsigned checks{},failures{};
inline void Check(bool condition,char const* name){++checks;if(!condition){++failures;std::printf("FAIL %s\n",name);}}
inline int Finish(char const* suite){std::printf("%s: %u checks, %u failures\n",suite,checks,failures);return failures?1:0;}
