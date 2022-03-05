/***
 * 
 * The media definition file for MediaPlayer MediaPlayer 
 * Version 0.10, February 2022
 * 
 * This file is a part of the media clip player for the PTMSC Pinto Abalone 
 * exhibit. See the file MediaPlayer.c for general information.
 * 
 * This file describes the video file used by the exhibit to play clips as 
 * needed as the visior interacts with the exhibit. All of the clips are 
 * contained in a single video file. A clip is described by the times at which 
 * it begins and ends, in ms relative to the beginning of the video, together 
 * with information describing how it should be handled -- looped, played 
 * through only once and so on. A clip description is of clip_t. The complete 
 * collection of clips in the video file is in the array named clips.
 * 
 ***
 * 
 * Copyright (C) 2020-2022 D.L. Ehnebuske
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE. 
 * 
***/
#define MEDIA_PATH      "/home/pi/Downloads/Sequence03Low.mp4" // The video containing all the clips for the exhibit.

enum clipNames {
    idle,               // This is the idle loop clip -- the clip that plays when nothing else is
    decide1,            // This clip shows a diver inspecting an outplacement site to see if it's a good one
    place1,             // This clip shows a diver doing an outplacement
    place2,             // This clip shows a diver doing an outplacement
    miss,               // This clip shows a diver doing an outplacement incorrectly
    decide2,            // This clip shows a diver inspecting a potntial site to see if it's a good one
    CLIP_COUNT          // This is not a clip name. Its value is the number of clips. It must be last.
};

enum clipTypes {
    playOnce,           // Play the clip once and then revert to idle. It's okay to interrupt it with an new clip
    playThrough,        // Play the clip once and the revert to idle. No new clip until it finishes
    loop                // Play the clip over and over. It's okay to interrupt it with a new clip
};

typedef struct clip_t {
    enum clipNames clipName;        // One of the clipNames
    enum clipTypes clipType;        // Which of the clipTypes this is 
    libvlc_time_t start;            // Time clip starts (ms)
    libvlc_time_t end;              // Time clip ends (ms)
} clip_t;

// The collection clip definitions
clip_t clips[] = {
    {idle, loop, 0, 13300}, 
    {decide1, playOnce, 13300, 34667},
    {place1, playOnce, 34667, 71533},
    {place2, playOnce, 71533, 93267},
    {miss, playOnce, 93267, 106367},
    {decide2, playOnce, 106367, 119000}
};
