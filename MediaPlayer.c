/***
 * MediaPlayer Version 0.10, February 2022
 * 
 * The media clip player for the PTMSC Pinto Abalone exhibit.
 * 
 * The media player plays video clips in response to commands from the main 
 * controller for the exhibit. Since it's attached to the exhibit's monitor 
 * and runs on a Pi, it also serves as a keyboard, mouse and monitor 
 * interface to the system. It can interpret a set of commands typed to it 
 * and will forward commands to the controller and display the controller's 
 * response.
 * 
 * The clips are all contained in one big video file. The location of the file 
 * and the description of the clips are contained in the file mediadf.h See 
 * there for details, but basically a clip is defined by its start and end 
 * time and information on how it is to be played. All of the complicated 
 * stuff about actually playing videos is handled by the open source VLC media 
 * player. From that point of view, MediaPlayer is nothing more than a custom 
 * interface adapted to our purpose that's built on top of VLC.
 * 
 * Media player's operation -- e.g., which clip it plays -- is done by sending 
 * it commands. Commands can come from the keyboard attached to stdin  (if any)
 * or they can come from the controller attached to CONTROLLER_TTY. Commands 
 * from the controller begin with "!" to distinguish them from diagnostic and 
 * informational messages from the controller. All messages from the 
 * controller, prefixed by "[controller] ", are echoed on stdout.
 * 
 * A line of inout from the keyboard is interpreted to be a command directed 
 * at MediaPlayer unless it starts with "!" in which case it is stripped of 
 * its leading "!" and sent to the controller. The idea is to allow the person 
 * at the keyboard to directly issue commands to the controller.
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
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <time.h>
#include <wiringPi.h>
#include <vlc/vlc.h>


#include "mediadef.h"                               // Definition of the media clips

#define CONTROLLER_TTY  "/dev/ttyACM0"              // The tty we use to talk to the exhibit controller
#define MAX_LINE_LENGTH (128)                       // The maximum length of a user's input (chars)
#define MAX_WORDS       (3)                         // The maximum number of words in a command line (chars)
#define MAX_WSIZE       (20)                        // The maximum length of a word (chars)
#define SLEEP_MICROS    (10000)                     // Number of uSec to sleep when a little time needs to pass
#define BANNER          "PTMSC Pinto Abalone Exhibit Media Player v0.1, February 2022"
#define CMD_SET_VERS    (1000)                      // The version of the command set we speak with the controller
#define ESCAPE_SEC      (300)                       // Seconds of execution before we stop. Comment out to disable
#define DEBUG                                       // Uncomment to enable debugginh output

// piLock() / piUnlock() usage
#define LOCK_CLIP       (0)                         // piLock(0) is for changing clips
#define LOCK_LOOP       (1)                         // piLock(1) is for changing the loop to play when not playing a clip

// Return codes
#define RET_OK          (0)                         // Normal end
#define RET_MICF        (-1)                        // Media item creation failure
#define RET_MPCF        (-2)                        // Media player creation failure
#define RET_MPPF        (-3)                        // Media player play failure
#define RET_KTCF        (-4)                        // Keyboard thread creation failure
#define RET_CTCF        (-5)                        // Controller thread creation failure
#define RET_OCTF        (-6)                        // Open controller TTY failure

/***
 * 
 * Global variables
 * 
 ***/
FILE *ctlIn;                                        // The input stream for the exhibit controller
FILE *ctlOut;                                       // The output stream for the controller
libvlc_instance_t * inst;                           // The libVLC engine we'll be using
libvlc_media_player_t *mp;                          // The media player we'll use
libvlc_media_t *m[CLIP_COUNT];                      // The clips we'll play represented as media items
bool running = true;                                // When this goes false (e.g., the stop command), we shut down
bool isFullscreen =                                 // Whether we display the video in fullscreen mode
#ifdef DEBUG 
false; 
#else 
true; 
#endif

// Inter-thread communication between a thread that wants to change the clip that's playing and 
// main loop. To change the clip, do a piLock(LOCK_CLIP), then set newClipId to the number of the 
// desired clip. Then set switchClip to true and do a piUnlock(LOCK_CLIP). The main loop checks 
// switchClip to determine if someone has set it. If so it does a piLock(LOCK_CLIP), copies 
// newClipId, sets switchClip to false and  does a piUnlock(LOCK_CLIP). Note that with this 
// protocol, it's possible to overwrite somebody else's clip change before it's processed. If 
// that's not what what you want, check switchClip to see that it's false before you change 
// newClipId.
int newClipId;
bool switchClip = false;
// Inter-thread communication for changing the loop that plays when there's no clip playing. Works 
// the same way as the above but using piLock(LOCK_LOOP), newLoopId and switchLoop
int newLoopId;
bool switchLoop = false;

/***
 * 
 * Command handler for help command
 * 
 ***/
void onHelp(int n, char word[MAX_WORDS][MAX_WSIZE]) {
    puts(
        "help           Type this help text.\n"
        "h              Same as help\n"
    );
    puts(
        "play <cName>   Play clip with name <cName>"
        "stop           Shutdown the media player\n"
    );
}

/***
 * 
 * Command handler for play command
 * 
 * play cName   Play clip cName; where cName is one of
 *              the clips[].name entries
 ***/
void onPlay(int n, char word[MAX_WORDS][MAX_WSIZE]) {
    if (n < 2) {
        puts("Clip name not specified.\n");
        return;
    }
    for (int cNo = 0; cNo < CLIP_COUNT; cNo++) {
        if (strcmp(word[1], clips[cNo].name) == 0) {
            piLock(LOCK_CLIP);      // Get the lock
            newClipId = cNo;
            switchClip = true;
            piUnlock(LOCK_CLIP);    // Release the lock
            return;
        }
    }
    printf("No clip named \"%s\"\n", word[1]);
}

/***
 * 
 * Command handler for the !playClip command, issued by controller
 * 
 * !playClip clipId
 *      Play the clip whose clip id -- the index into 
 *      clips[] -- is clipId
 ***/
void onPlayClip(int n, char word[MAX_WORDS][MAX_WSIZE]) {
    int clipId = 0;
    if (n < 2) {
        puts("!playClip inviked with no clipId specified; used 0.\n");
    } else {
        clipId = atoi(word[1]);
        if (clipId < 0 || clipId >= CLIP_COUNT) {
            printf("!playClip inviked with invalid clipId: \"%s\"; used .", word[1]);
            clipId = 0;
        }
    }
    piLock(LOCK_CLIP);      // Get the lock
    newClipId = clipId;
    switchClip = true;
    piUnlock(LOCK_CLIP);    // Release the lock
}

/***
 * 
 * Command handler for !setLoop command, issued by controller
 * 
 * !setLoop clipId
 *      Set the video loop to be played when we're not playing something else
 *      to the clip whose id -- the index into clips[] -- is clipId. 
 *      If a loop is already playing, switch to playing this loop instead.
 ***/
void onSetLoop(int n, char word[MAX_WORDS][MAX_WSIZE]) {
    int clipId = 0;
    if (n < 2) {
        puts("!setLoop invoked with no clipId specified; used 0.\n");
    } else {
        clipId = atoi(word[1]);
        if (clipId < 0 || clipId >= CLIP_COUNT) {
            printf("!setLoop invoked with invalid clipId: \"%s\"; used 0.", word[1]);
            clipId = 0;
        }
    }
    piLock(LOCK_LOOP);      // Get the lock
    newLoopId = clipId;
    switchLoop = true;
    piUnlock(LOCK_LOOP);    // Release the lock
}

/***
 * 
 * Command handler for stop command
 * 
 * stop     Stops media player and exits program
 * !stop    Same as stop, but issued from controller
 * 
 ****/
void onStop(int n, char word[MAX_WORDS][MAX_WSIZE]) {
    puts("Stopping\n");
    running = false;
}

/***
 *
 *  Command handler for !toggleFS command
 *
 *  !toggleFS   Toggle whether the clips are playing in fullscreen mode
 *              Only issued by controller
 *
 ***/
 void onToggleFS(int n, char word[MAX_WORDS][MAX_WSIZE]) {
     if (mp == NULL) {
         puts("Ignoring !toggleFS command; no media player defined.");
         return;
     }
     isFullscreen = !isFullscreen;
     libvlc_set_fullscreen(mp, isFullscreen);
     printf("Screen mode set to %s.\n", isFullscreen ? "full" : "window");
 }

/***
 * 
 * Command handler for !version command
 * 
 * !version     Send MediaPlayer command set version information to controller
 *              Only issued by controller
 * 
 ***/
void onVersion(int n, char word[MAX_WORDS][MAX_WSIZE]) {
    fprintf(ctlOut, "!mediaplayer %d\n", CMD_SET_VERS);         // Tell controller what command set we speak
}

// Command registry data structure
typedef struct cmd_t {
    char cmd[MAX_WSIZE];                                        // The command name
    void (*handler)(int n, char word[MAX_WORDS][MAX_WSIZE]);    // The command handler for this command
} cmd_t;

// The registry of keyboard-issued commands aimed at MediaPlayer. 
// The last command must be {"__END__", NULL}.
cmd_t kbRegistry[] = {
    {"help", onHelp},
    {"h",    onHelp},
    {"play", onPlay},
    {"stop", onStop},
    {"__END__", NULL}
};

// The registry of controller-issued commands aimed at MediaPlayer. 
// The last command must be {"__END__", NULL}.
cmd_t controllerRegistry[] = {
    {"!playClip", onPlayClip},
    {"!setLoop", onSetLoop},
    {"!stop", onStop},
    {"!toggleFS", onToggleFS},
    {"!version", onVersion},
    {"__END__", NULL}
};

/***
 * 
 * doCommand execute the command in line using the command registry in registry
 * 
 ***/
void doCommand(char line[], cmd_t registry[]) {
    char word[3][20];
    int nparms = sscanf(line, "%20s %20s %20s", word[0], word[1], word[2]);
    if (nparms != EOF) {
        for (int i = 0; registry[i].handler != NULL; i++) {
            if (strcmp(registry[i].cmd, word[0]) == 0) {
                (registry[i].handler)(nparms, word);
                break;
            }
        }
    }
}

/***
 * 
 * keyboardThread -- communicate with someone at the keyboard on stdin and stdout via 
 * a simple, application-specific commandline
 * 
 * Commands up to three white-space-separated words, the first of which is the name of the
 * command to be executed. The others, if any, are the parameters passed to the command.
 * 
 ***/
PI_THREAD(keyboardThread) {
    static char buffer[MAX_LINE_LENGTH];
    printf("> ");
    while (1==1){
        if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
            // Send commands beginning with '!' to controller (minus the '!'); others are local
            if (buffer[0] == '!') {
                printf("Sending \"%s\" to controller", &buffer[1]);
                fprintf(ctlOut, &buffer[1]);
            } else {
                doCommand(buffer, kbRegistry);
            }
        }
        printf("> ");
    }
}

/***
 * 
 * controllerThread -- get input from the exhibit controller. Echo whatever it says to stdout, 
 * prefaced by "[controller] ". Watch for lines beginning with "!" which indicates a command 
 * directed at MediaPlayer, to be executed using the same sort of mechanism (and the same handler 
 * signatures) and the keyboard commands.
 * 
 ***/
PI_THREAD(controllerThread) {
    char buffer[MAX_LINE_LENGTH];

    while (1==1) {
        if (fgets(buffer, sizeof(buffer), ctlIn) != NULL) {
            printf("[controller] %s", buffer);
            if (buffer[0] == '!') {
                doCommand(buffer, controllerRegistry);
            }
        }
    }
}

/***
 * 
 * main     What gets called to kick things off and returns to shut things down
 * 
 ***/
int main(int argc, char* argv[]) {
    int reqClipId;                                  // The id of the requested clip; 0 if none
    int reqLoopId = 0;                              // The id of the clip that plays when no clip is playing
    int nowPlayingId = 0;                           // The id of the clip the media player was last started on

    // Show we're alive
    puts(BANNER);
    puts("Type \"help\" for list of commands");

    // Get the keyboard input thread going. All stdin activity is done on keyboardThread
    // stdout and ctlOut activity can be done by any thread.
    if (piThreadCreate(keyboardThread) != 0) {
        puts("Failed to create keyboard thread.");
        return RET_KTCF;
    }

    // Get the connections to the exhibit controller (ctlIn and ctlOut) going
    // ctlIn needs line buffering mode and no echoing of characters
    ctlIn = fopen(CONTROLLER_TTY, "r");
    if (ctlIn == NULL) {
        printf("Failed to open ctlIn. Error: %s\n", strerror(errno));
        return RET_OCTF;
    }
    if (setvbuf(ctlIn, NULL, _IOLBF, MAX_LINE_LENGTH) != 0) {
        printf("Failed to set ctlIn buffer mode. Error: %s\n", strerror(errno));
        return RET_OCTF;
    }
    struct termios t;
    if (tcgetattr(ctlIn->_fileno, &t) != 0) {
        printf("Failed to get termios for ctlIn. Error: %d, (%s)\n", errno, strerror(errno));
        return RET_OCTF;
    }
    t.c_lflag &= ~ECHO;
    if (tcsetattr(ctlIn->_fileno, TCSANOW, &t) != 0) {
        printf("Failed to set termios for ctlIn. Error: %d, (%s)\n", errno, strerror(errno));
        return RET_OCTF;
    }

    // ctlOut needs append mode
    ctlOut = fopen(CONTROLLER_TTY, "a");
    if (ctlOut == NULL) {
        printf("Failed to open ctlOut. Error: %s\n", strerror(errno));
        return RET_OCTF;
    }

    // Get the controller thread going all ctlIn activity is done on controllerThread
    if (piThreadCreate(controllerThread) != 0) {
        puts("Failed to create controller thread.");
        return RET_CTCF;
    }

    // Set things up to play the exhibit's media
    inst = libvlc_new(0, NULL);

    for (int cNo = 0; cNo < CLIP_COUNT; cNo++) {
        char path[sizeof(MEDIA_PATH) + CLIP_FILE_MAX] = MEDIA_PATH;
        strcat(path, clips[cNo].file);
        m[cNo] = libvlc_media_new_path(inst, path);             // Make a media item pointing to MEDIA_PATH
        if (m[cNo] == NULL) {                                   // Check that it worked
            printf("Failed to create clip media item %d\n", cNo);
            return RET_MICF;
        }
    }

    // Instantiate the media player
    mp = libvlc_media_player_new(inst);
    if (mp == NULL) {
        puts("Failed to create media player");
        return RET_MPCF;
    }
    puts("Ready to go. Waiting word from controller.");
    while (!switchLoop && !switchClip && running) {
        usleep(SLEEP_MICROS);                                   // Wait for controller to kick things off (or stop command)
    }

    // Main loop. Do until running goes false
    // There are three key variables here. reqLoopId, the id of the looping clip to play (0 if none) when there no specific 
    // clip has been requested; reqClipId, the id of the last specifically requested clip (0 if none); and nowPlayingId, the 
    // id of the clip the media player was last tasked to play.
    // There are two types of clips that can be requested, playOnce and PlayThrough. playOnce clips should be interrupted
    // if they are playing when a new request is received. A playThrough clip plays to the end before a newly requested clip
    // starts.
    while (running) {
        if (switchLoop) {                                           // If we've been told to switch which clip is the looping one
            int oldLoopId = reqLoopId;
            piLock(LOCK_LOOP);                                      //   Do the ritual to update what the requested looping clip is
            reqLoopId = newLoopId;
            switchLoop = false;
            piUnlock(LOCK_LOOP);
            if (reqLoopId < 0 || reqLoopId >= sizeof(clips) / sizeof(clips[0])) {
                printf("Controller asked for non-existant loop: %d. Ignoring request.\n", reqLoopId);
                reqLoopId = oldLoopId;
            } else if (clips[reqLoopId].type != loop) {             //  Otherwise if the new requested clip isn't looping, ignore the request
                printf("Ignoring request to loop non-looping clip %s\n", clips[reqLoopId].name);
                reqLoopId = oldLoopId;
            } else {                                                //   Otherwise (make the switch to the new one)
                if (nowPlayingId == oldLoopId) {                    //     If current clip that's playing is the old looping clip
                    nowPlayingId = reqLoopId;                       //       Swap out the old looping clip with the new one
                    libvlc_media_player_pause(mp);                  //       Pause the playing (so the player is out of work)
                }
                printf("Switching looping clip to %d (%s)\n", reqLoopId, clips[reqLoopId].name);
            }
        }
        if (switchClip && reqClipId == 0) {                         // If we've been told to play a new clip and there's not one already queued
            int oldClipId = reqClipId;
            piLock(LOCK_CLIP);                                      //   Do the ritual to switch which clip is current
            reqClipId = newClipId;
            switchClip = false;
            piUnlock(LOCK_CLIP);
            printf("Switching to clip %d (%s)\n", reqClipId, clips[reqClipId].name);
            if (reqClipId < 0 || reqClipId >= sizeof(clips) / sizeof(clips[0])) {
                printf("Controller asked for non-existent clip: %d. Ignoring request.\n", reqClipId);
                reqClipId - oldClipId;
            } else if (clips[nowPlayingId].type != playThrough && libvlc_media_player_is_playing(mp)) {
                                                                    //   If what's playing is interruptable and the media player is playing
                libvlc_media_player_pause(mp);                      //     Pause the player (so that it's out of work)
            }
            #ifdef TRAP
            if (reqClipId == 3) {                                   //  if it's "abandonedClip"
                running = false;                                    //    Bail
            }
            #endif
        }
        #ifdef ESCAPE_SEC
        //Escape hatch
        if (clock() / CLOCKS_PER_SEC >= ESCAPE_SEC) {
            puts("Stopping: Escape hatch activated.");
            running = false;
        } else
        #endif
        if (!libvlc_media_player_is_playing(mp)) {                  // If the player is out of work
            if (clips[nowPlayingId].type != loop) {                 //   If what's been playing a looping clip (i.e., it was requested)
                printf("Finished clip %d (%s)\n", nowPlayingId, clips[nowPlayingId].name);
                fprintf(ctlOut, "!videoEnds\n");                    //     Let the controller know the clip finished
                if (ferror(ctlOut)) {
                    printf("!videoEnds fprintf error: %s\n", strerror(errno));
                }
            }
            if (reqClipId != 0) {                                   //   If there's a requested clip pending
                nowPlayingId = reqClipId;                           //     Switch to the requested clip
                reqClipId = 0;                                      //     Mark that we've go it handled
                printf("Starting clip %d (%s)\n", nowPlayingId, clips[nowPlayingId].name);
            } else {                                                //   Otherwise (there wasn't a pending clip play request)
                nowPlayingId = reqLoopId;                           //     Play the looping clip
            }
            libvlc_media_player_set_media(mp, m[nowPlayingId]);     //   Tell the player we want to play the nowPlayingId clip
            if (libvlc_media_player_play(mp) != 0) {                //   Try to start playing the clip. If that fails
                libvlc_set_fullscreen(mp, false);                   //     Get out of fullscreen mode
                puts("Failed to start clip. Stopping");             //     Bail out
                running = false;
            }
            while (!libvlc_media_player_is_playing(mp)) {           //   Spin until it gets going
                usleep(SLEEP_MICROS);
            }
        }
        usleep(SLEEP_MICROS);                                       // Mostly, we sleep
    }

    puts("Cleaning up.");
    // Quitting time. Clean up after ourselves
    for (int cNo = 0; cNo < CLIP_COUNT; cNo++) {    // Release the media items
        libvlc_media_release (m[cNo]);
    }
    libvlc_media_player_stop(mp);                   // Stop the media player
    libvlc_set_fullscreen(mp, false);               // Take it out of fullscreen mode
    libvlc_media_player_release(mp);                // Release it
    libvlc_release(inst);                           // Then release the engine
    puts("Exiting MediaPlayer");
    return RET_OK;                                  // End normally
}