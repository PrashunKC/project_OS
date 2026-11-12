#pragma once
#include "stdint.h"

// Round number up to the next multiple of alignTo; zero leaves number unchanged.
uint32_t align(uint32_t number, uint32_t alignTo);
