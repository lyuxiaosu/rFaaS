#include <cstdint>

uint32_t
fib(uint32_t n)
{
        if (n <= 1) return n;
        return fib(n - 1) + fib(n - 2);
}


extern "C" uint32_t fibonacci(void* args, uint32_t size, void* res)
{
  uint32_t* input = static_cast<uint32_t*>(args);
  uint32_t* result = static_cast<uint32_t*>(res);

  *result = fib(*input);
  
  return sizeof(uint32_t);
}

