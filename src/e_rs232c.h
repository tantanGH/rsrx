#ifndef __H_HIMEM__
#define __H_HIMEM__

#include <stdint.h>
#include <stddef.h>

void e_set232c(int32_t mode);
uint8_t* e_buf232c(uint8_t* buf_addr, size_t buf_size, size_t* orig_size);
void e_out232c(uint8_t data);
int32_t e_inp232c();
int32_t e_lof232c();
int32_t e_rs232c_isavailable(void);

#endif