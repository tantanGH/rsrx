#include <stdint.h>
#include <stddef.h>
#include <iocslib.h>
#include <doslib.h>
#include "e_rs232c.h"

void e_set232c(int32_t mode) {
    
  struct REGS in_regs = { 0 };
  struct REGS out_regs = { 0 };

  in_regs.d0 = 0xf1;
  in_regs.d1 = mode;
  in_regs.d2 = 0x0030;

  TRAP15(&in_regs, &out_regs);
}

uint8_t* e_buf232c(uint8_t* buf_addr, size_t buf_size, size_t* orig_size) {

  struct REGS in_regs = { 0 };
  struct REGS out_regs = { 0 };

  in_regs.d0 = 0xf1;
  in_regs.d1 = buf_size;
  in_regs.d2 = 0x0036;
  in_regs.a1 = (uint32_t)buf_addr;

  TRAP15(&in_regs, &out_regs);

  *orig_size = out_regs.d1;

  return (uint8_t*)out_regs.a1;
}

void e_out232c(uint8_t data) {
    
  struct REGS in_regs = { 0 };
  struct REGS out_regs = { 0 };

  in_regs.d0 = 0xf1;
  in_regs.d1 = data;
  in_regs.d2 = 0x0035;

  TRAP15(&in_regs, &out_regs);
}

int32_t e_inp232c() {

  struct REGS in_regs = { 0 };
  struct REGS out_regs = { 0 };

  in_regs.d0 = 0xf1;
  in_regs.d2 = 0x0032;

  TRAP15(&in_regs, &out_regs);

  return out_regs.d0 & 0xff;
}

int32_t e_lof232c() {

  struct REGS in_regs = { 0 };
  struct REGS out_regs = { 0 };

  in_regs.d0 = 0xf1;
  in_regs.d2 = 0x0031;

  TRAP15(&in_regs, &out_regs);

  return out_regs.d0 & 0xffff;
}

// check enhanced RS232C call availability
int32_t e_rs232c_isavailable() {
  int32_t v = INTVCG(0x1f1);
  return (v < 0 || (v >= 0xfe0000 && v <= 0xffffff)) ? 0 : 1;
}