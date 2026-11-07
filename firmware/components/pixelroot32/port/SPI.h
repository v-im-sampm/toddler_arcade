// Little Arcade port shim: DisplayConfig.h includes <SPI.h> on non-native builds and
// relies on the TFT_eSPI driver header to pull in DrawSurface; neither exists here.
#pragma once
#include "graphics/DrawSurface.h"

