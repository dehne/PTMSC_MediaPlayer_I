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
#include <wiringPi.h>
#include <vlc/vlc.h>

#include "mediadef.h"                               // Definition of the media clip

#define CONTROLLER_TTY  "/dev/ttyACM0"              // The tty we use to talk to the exhibit controller
#define MAX_LINE_LENGTH (128)                       // The maximum length of a user's input (chars)
#define MAX_WORDS       (3)                         // The maximum number of words in a command line (chars)
#define MAX_WSIZE       (20)                        // The maximum length of a word (chars)
#define SLEEP_US        (100)                       // Number of uSec to sleep whe some time needs to pass (1/30 sec)
#define MEDIA_DELTA_MS  (600)                       // The Adobe Rush times - VLC times in ms (18 frames)
#define BANNER          "PTMSC Pinto Abalone Exhibit Media Player v0.1, February 2022"
#define CMD_SET_VERS    (1000)                      // The version of the command set we speak with the controller

// piLock() / piUnlock() usage
#define LOCK_CLIP       (0)                         // piLock(0) is for changing clips

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
bool running = true;                                // When this goes false (e.g., the stop command), we shut down

// Inter-thread communication between a thread that wants to change the clip that's playing and 
// main loop. To change the clip, do a piLock(LOCK_CLIP), then set newClip to the desired clip. 
// Then set switchClip to true and do a piUnlock(LOCK_CLIP). The main loop checks switchClip to 
// determine if someone has set it. If so it does a piLock(LOCK_CLIP), copies newClip, sets 
// switchClip to false and  does a piUnlock(LOCK_CLIP). Note that with this protocol, it's 
// possible to overwrite somebody else's clip change before it's processed. If that's not what 
// what you want, check switchClip to see that it's false before you change newClip.
clip_t newClip;
bool switchClip = false;

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
    printf(
        "play <c>       Play clip c; 0 <= c < %d\n", CLIP_COUNT
    );
    puts(
        "stop           Shutdow the media player\n"
    );
}

/***
 * 
 * Command handler for play command
 * 
 * play c   Play clip c; 0 <= c < CLIP_COUNT
 * !play c  Same as play, but issued by controller
 * 
 ***/
void onPlay(int n, char word[MAX_WORDS][MAX_WSIZE]) {
    if (n < 2) {
        puts("Clip number not specified.\n");
        return;
    }
    int c = -1;
    if (sscanf(word[1], "%d", &c) != 1 || c < 0 || c >= CLIP_COUNT) {
        printf("Clip number must be 0 to %d\n", CLIP_COUNT);
        return;
    }
    piLock(LOCK_CLIP);      // Get the lock
    newClip = clips[c];
    switchClip = true;
    piUnlock(LOCK_CLIP);    // Release the lock
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
    {"!play", onPlay},
    {"!stop", onStop},
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
            // Send commands beginning with '!' to controller; others are local
            if (buffer[0] == '!') {
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
    libvlc_instance_t * inst;                       // The libVLC engine we'll be using
    libvlc_media_player_t *mp;                      // The media player we'll use
    libvlc_media_t *m;                              // The media item we'll play
    clip_t curClip;                                 // The clip that's currently playing

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
        return(RET_OCTF);
    }
    t.c_lflag &= ~ECHO;
    if (tcsetattr(ctlIn->_fileno, TCSANOW, &t) != 0) {
        printf("Failed to set termios for ctlIn. Error: %d, (%s)\n", errno, strerror(errno));
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
    inst = libvlc_new (0, NULL);                    // Load and initialize the libVLC engine
  
    m = libvlc_media_new_path (inst, MEDIA_PATH);
                                                    // Make the media item pointing to what we want to play

    if (m == NULL) {                                // Check that it worked
        puts("Failed to create clip media item");
        return RET_MICF;
    }
        
    mp = libvlc_media_player_new_from_media (m);    // Create a new media player to play the item

    if (mp == NULL) {                               // Check that it worked
        puts("Failed to create media player");
        return RET_MPCF;
    }
     
    libvlc_media_release (m);                       // Indicate we have no further use for the media item

    // Get the clip up and running
    if (libvlc_media_player_play (mp) != 0) {           // Kick it off
        puts("Media player failed to play");
        return RET_MPPF;
    }
    while (!libvlc_media_player_is_playing(mp)) {       // Wait until it gets going
        usleep(SLEEP_US);                               // sleep for a bit
    }

    newClip = curClip = clips[idle];

    puts("MediaPlayer initialized and running.");

    // Main loop. Do until running goes false
    while (running) {
        if (switchClip) {
            piLock(LOCK_CLIP);
            curClip = newClip;
            switchClip = false;
            piUnlock(LOCK_CLIP);
            libvlc_media_player_set_time(mp, curClip.start);
        }
        libvlc_time_t curTime = libvlc_media_player_get_time(mp) + MEDIA_DELTA_MS;
        if (curTime >= curClip.end) {
            printf("Current time: %jd, Clip end: %jd\n", curTime, curClip.end);
            if (curClip.clipType != loop) {
                curClip = clips[idle];
            }
            libvlc_media_player_set_time(mp, curClip.start);
        }
        usleep(SLEEP_US);               // Sleep for a bit
    }

    // Quitting time. Clean up after ourselves
    libvlc_media_player_stop(mp);       // Stop the media player
    libvlc_media_player_release(mp);    // Release it
    libvlc_release(inst);               // Then release the engine
    return RET_OK;                      // All very normal
}