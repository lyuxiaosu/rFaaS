#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <stdint.h>

extern "C" uint32_t trap(void* args, uint32_t size, void* res)
{
        uint32_t* input = static_cast<uint32_t*>(args);
        uint32_t* result = static_cast<uint32_t*>(res);
        if (*input == 2) {
            /* Illegal memory operation */
            uint32_t *trap = NULL;
            *trap = 42;
        } else if (*input == 1) {
            /*Infinite loop */
            while(1) {};
        } else if (*input == 3) {
            /*ILLEGAL_ARITHMETIC*/
            uint32_t trap = 10 / 0;
        } else {
        }
        *result = 4;
        return sizeof(uint32_t); 
}

