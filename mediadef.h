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

// The collection clip definitions, indexed by the type sb_clipId_t
clip_t clips[] = {
    {"noClip", "dummy.mp4", playOnce},
    {"divingLoop", "divingLoop.mp4", loop},
    {"restingLoop", "restingLoop.mp4", loop}, 
    {"abandonedClip", "abandonedClip.mp4", playOnce}, 
    {"fullSiteClip1", "fullSite1Clip.mp4", playOnce}, 
    {"fullSiteClip2", "fullSite2Clip.mp4", playOnce}, 
    {"fullSiteClip3", "fullSite3Clip.mp4", playOnce}, 
    {"fullSiteClip4", "fullSite4Clip.mp4", playOnce}, 
    {"fullSiteClip5", "fullSite5Clip.mp4", playOnce},
    {"siteNoCohortsClip1", "siteNoCohorts1Clip.mp4", playOnce}, 
    {"siteNoCohortsClip2", "siteNoCohorts2Clip.mp4", playOnce}, 
    {"siteNoCohortsClip3", "siteNoCohorts3Clip.mp4", playOnce}, 
    {"siteNoCohortsClip4", "siteNoCohorts4Clip.mp4", playOnce}, 
    {"siteNoCohortsClip5", "siteNoCohorts5Clip.mp4", playOnce},
    {"openSiteLoop1", "openSite1Loop.mp4", loop}, 
    {"openSiteLoop2", "openSite2Loop.mp4", loop}, 
    {"openSiteLoop3", "openSite3Loop.mp4", loop}, 
    {"openSiteLoop4", "openSite4Loop.mp4", loop}, 
    {"openSiteLoop5", "openSite5Loop.mp4", loop}, 
    {"fillSiteClip1", "fillSite1Clip.mp4", playOnce}, 
    {"fillSiteClip2", "fillSite2Clip.mp4", playOnce}, 
    {"fillSiteClip3", "fillSite3Clip.mp4", playOnce}, 
    {"fillSiteClip4", "fillSite4Clip.mp4", playOnce}, 
    {"fillSiteClip5", "fillSite5Clip.mp4", playOnce},
    {"boatCohortsClip", "boatCohortsClip.mp4", playOnce}, 
    {"outplantedClip", "outplantedClip.mp4", playOnce}, 
    {"transitionClip", "transitionClip.mp4", playOnce}
};