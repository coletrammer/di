#pragma once

#ifdef __x86_64__
#define DI_X86_64
#elifdef __arm64__
#define DI_ARM64
#endif
