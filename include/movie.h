#ifndef MOVIE_H
#define MOVIE_H

#include <stdbool.h>

/* Player dos .MOV (MOV2) do Exceed — ver src/movie.c */
bool Movie_Open(const char* path, bool loop);
void Movie_Close(void);
bool Movie_IsOpen(void);
bool Movie_HasEnded(void);
void Movie_Update(float dt);
void Movie_Render(void);

#endif
