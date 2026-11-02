#pragma once

#include <stdint.h>

void pic_remap(void);
void pic_mask_all(void);
void pic_unmask(uint8_t irq);
void pic_eoi(uint8_t irq);
