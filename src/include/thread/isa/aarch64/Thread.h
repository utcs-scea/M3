#pragma once

#ifdef __cplusplus
#include <base/Types.h>

namespace m3 {

typedef void (*_thread_func)(void*);

struct Regs {
  word_t   x0;
  word_t  x19;
  word_t  x20;
  word_t  x21;
  word_t  x22;
  word_t  x23;
  word_t  x24;
  word_t  x25;
  word_t  x26;
  word_t  x27;
  word_t  x28;
  word_t   fp;  //x29
  word_t   lr;  //x30
  word_t   sp;  //x31
  word_t DAIF;
  word_t NZCV;
};

enum {
    T_STACK_WORDS = 512
    //T_STACK_WORDS = 1024
};

void thread_init(_thread_func func, void *arg, Regs *regs, word_t *stack);
extern "C"  bool thread_save(Regs *regs);
extern "C" bool thread_resume(Regs *regs);

}

#endif
