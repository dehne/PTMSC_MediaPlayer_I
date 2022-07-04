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
#pragma once

// The MRL for the video containing all the clips for the exhibit.
#define MEDIA_PATH      "/home/pi/Downloads/"
#define CLIP_COUNT      (sizeof(clips) / sizeof(clips[0]))  // Number of clips we have
#define CLIP_NAME_MAX   (18)                                // Maximum number of chars in clip_t name
#define CLIP_FILE_MAX   (30)                                // Maximum number of chars in clip_t file

enum clipTypes {
    playOnce,           // Play the clip once and then revert to idle. It's okay to interrupt it with an new clip
    playThrough,        // Play the clip once and the revert to idle. New clips ignored until it finishes
    loop                // Play the clip over and over. It's okay to interrupt it with a new clip
};

typedef struct clip_t {
    char name[CLIP_NAME_MAX];                               // Name of clip
    char file[CLIP_FILE_MAX];                               // Filename relative to MEDIAPATH
    enum clipTypes type;                                    // Type of clip
} clip_t;

// The collection clip definitions, indexed by the type sb_clipId_t in Storyboardtypes.h over in
// the controller program. This needs to match exactly.
clip_t clips[] = {
    {"noClip", "dummy.mp4", playOnce},                          //  0
    {"divingLoop", "divingLoop.mp4", loop},                     //  1
    {"restingLoop", "restingLoop.mp4", loop},                   //  2
    {"abandonedClip", "abandonedClip.mp4", loop},               //  3
    {"instructLoop", "instructLoop.mp4", loop},                 //  4
    {"fullSite1Clip", "fullSite1Clip.mp4", playOnce},           //  5
    {"fullSite2Clip", "fullSite2Clip.mp4", playOnce},           //  6
    {"fullSite3Clip", "fullSite3Clip.mp4", playOnce},           //  7
    {"fullSite4Clip", "fullSite4Clip.mp4", playOnce},           //  8
    {"fullSite5Clip", "fullSite5Clip.mp4", playOnce},           //  9
    {"site1NoCohortsClip", "site1NoCohortsClip.mp4", playOnce}, // 10
    {"site2NoCohortsClip", "site2NoCohortsClip.mp4", playOnce}, // 11
    {"site3NoCohortsClip", "site3NoCohortsClip.mp4", playOnce}, // 12
    {"site4NoCohortsClip", "site4NoCohortsClip.mp4", playOnce}, // 13
    {"site5NoCohortsClip", "site5NoCohortsClip.mp4", playOnce}, // 14
    {"openSite1Clip", "openSite1Clip.mp4", playOnce},           // 15
    {"openSite2Clip", "openSite2Clip.mp4", playOnce},           // 16
    {"openSite3Clip", "openSite3Clip.mp4", playOnce},           // 17
    {"openSite4Clip", "openSite4Clip.mp4", playOnce},           // 18
    {"openSite5Clip", "openSite5Clip.mp4", playOnce},           // 10
    {"fillSite1Clip", "fillSite1Clip.mp4", playOnce},           // 20
    {"fillSite2Clip", "fillSite2Clip.mp4", playOnce},           // 21
    {"fillSite3Clip", "fillSite3Clip.mp4", playOnce},           // 22
    {"fillSite4Clip", "fillSite4Clip.mp4", playOnce},           // 23
    {"fillSite5Clip", "fillSite5Clip.mp4", playOnce},           // 24
    {"atSite1Loop", "atSite1Loop.mp4", loop},                   // 25
    {"atSite2Loop", "atSite2Loop.mp4", loop},                   // 26
    {"atSite3Loop", "atSite3Loop.mp4", loop},                   // 27
    {"atSite4Loop", "atSite4Loop.mp4", loop},                   // 28
    {"atSite5Loop", "atSite5Loop.mp4", loop},                   // 29
    {"reviewSite1Clip", "reviewSite1Clip.mp4", playOnce},       // 30
    {"reviewSite2Clip", "reviewSite2Clip.mp4", playOnce},       // 31
    {"reviewSite3Clip", "reviewSite3Clip.mp4", playOnce},       // 32
    {"reviewSite4Clip", "reviewSite4Clip.mp4", playOnce},       // 33
    {"reviewSite5Clip", "reviewSite5Clip.mp4", playOnce},       // 34
    {"outAtSite1Clip", "outAtSite1Clip.mp4", playOnce},         // 35
    {"outAtSite2Clip", "outAtSite2Clip.mp4", playOnce},         // 36
    {"outAtSite3Clip", "outAtSite3Clip.mp4", playOnce},         // 37
    {"outAtSite4Clip", "outAtSite4Clip.mp4", playOnce},         // 38
    {"outAtSite5Clip", "outAtSite5Clip.mp4", playOnce},         // 39
    {"boatCohortsLoop", "boatCohortsLoop.mp4", loop},           // 40
    {"atBoatLoopp", "atBoatLoop.mp4", loop},                    // 41
    {"transitionClip", "transitionClip.mp4", playOnce},         // 42
    {"calibrationLoop", "calibrateLoop.mp4", loop},             // 43
    {"reviewIntroClip", "reviewIntroClip.mp4", playOnce},       // 44
    {"superScoreClip", "superScoreClip.mp4", playOnce},         // 45
    {"goodScoreClip", "gooScoreClip.mp4", playOnce},            // 46
    {"mehScoreClip", "mehScoreClip.mp4", playOnce}              // 47
};