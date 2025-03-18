#ifndef MONONOKE_VIDEO_H
#define MONONOKE_VIDEO_H

//! This file provides a C interface to the Mononoke
//! 86Box video driver implementation. This is needed since
//! the headless UI layer is currently mostly C. (Suprisingly, 
//! besides the 86box core, it's the largest chunk of C code specifically
//! in the headless port.)

#ifdef __cplusplus
extern "C" {
#endif

void mononoke_video_init(void* arg);
void mononoke_video_close();

void mononoke_video_resize(int x, int y);

#ifdef __cplusplus
}
#endif

#endif