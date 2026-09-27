#ifndef BGA_H
#define BGA_H

#include "pumpy.h"

bool isMenuOverlayLayer(BGALayer* layer);
bool isMenuArrowLayer(BGALayer* layer);
bool isMenuTextLayer(BGALayer* layer);
bool isMenuCenterLayer(BGALayer* layer);
bool layerMatchesDirection(BGALayer* layer, int sel);
int findBGALoopStart(void);
int findBGALoopEnd(void);

/* Exceed: camada por slot do arquivo (0x41F55C) e escala global (0x41F754) */
int BGA_DrawSlot(int bgaIndex, int frame, int slot);
void BGA_SetScale(int bgaIndex, float sx, float sy);
void BGA_SetColor(int bgaIndex, float rgb, float a);   /* 0x41F754 */

#endif
