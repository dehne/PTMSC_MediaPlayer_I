/***
 * 
 * MediaPlayer Version 0.10
 * 
 * The mediaplayer for the PTMSC Pinto Abalone exhibit.
 * 
 ***/
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <wiringPi.h>
#include <vlc/vlc.h>
#include <unistd.h>

#include "mediadef.h"                               // Definition of the media clip

#define CONTROLLER_TTY  "/dev/ttyACM0"              // The tty we use to talk to the exhibit controller
#define MAX_LINE_LENGTH (512)                       // The maximum length of a user's input (chars)
#define MAX_WORDS       (3)                         // The maximum number of words in a command line (chars)
#define MAX_WSIZE       (20)                        // The maximum length of a word (chars)
#define SLEEP_US        (100000)                    // Number of uSec to sleep whe some time needt to pass
#define BANNER          "PTMSC Pinto Abalone Exhibit Media Player v0.1, February 2022"
#define CMD_SET_VERS    (1000)                      // The version of the command set we speak with the controller

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
FILE *controllerTTY;                                // The stream over which we talk to the exhibit controller
bool running = true;                                // When this goes false (e.g., the stop command), we shut down

// Inter-thread communication between a thread that wants to change the clip that's playing and main loop.
// To change the clip, do a piLock(0), then set newClip to the desired clip. Then set switchClip to true 
// and do a piUnlock. The main loop checks switchClip to determine if someone has set it. If so it does a 
// piLock(0), copies newClip, sets switchClip ot false and does a piUnlock(0).
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
 * play c   Plays clip c; 0 <= c < CLIP_COUNT
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
    piLock(0);                  // Get the lock
    newClip = clips[c];
    switchClip = true;
    piUnlock(0);
}

/***
 * 
 * Command handler for stop command
 * 
 * stop     Stops media player and exits program
 * 
 ****/
void onStop(int n, char word[MAX_WORDS][MAX_WSIZE]) {
    puts("Stopping\n");
    running = false;
}


// Command registry data structure
typedef struct cmd_t {
    char cmd[MAX_WSIZE];                                        // The command name
    void (*handler)(int n, char word[MAX_WORDS][MAX_WSIZE]);    // The command handler for this command
} cmd_t;

// The registry of keyboard commands. The last command must be {"__END__", NULL}.
cmd_t kbRegistry[] = {
    {"help", onHelp},
    {"h",    onHelp},
    {"play", onPlay},
    {"stop", onStop},
    {"__END__", NULL}
};

// The registry of controller commands. The last command must be {"__END__", NULL}.
cmd_t controllerRegistry[] = {
    {"!play", onPlay},
    {"!stop", onStop},
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
                int rc = fprintf(controllerTTY, &buffer[1]); 
                if (rc < 0) {
                    printf("Couldn't send command to controller. rc = %d\n", rc);
                }
            } else {
                doCommand(buffer, kbRegistry);
            }
        }
        printf("> ");
    }
}

/***
 * 
 * controllerThread -- talk to the exhibit controller. Echo whatever it says to stdout, prefaced 
 * by "[controller] ". Watch for lines beginning with "!" which indicates a command for us, to be 
 * executed using the same sort of mechanism (and the same handler signatures) and the keyboard 
 * commands.
 * 
 ***/
PI_THREAD(controllerThread) {
    char buffer[MAX_LINE_LENGTH];

    while (1==1) {
        if (fgets(buffer, sizeof(buffer), controllerTTY) != NULL) {
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

    // Get the keyboard input thread going
    if (piThreadCreate(keyboardThread) != 0) {
        puts("Failed to create keyboard thread.");
        return RET_KTCF;
    }

    // Get the connection to the exhibit controller going
    controllerTTY = fopen(CONTROLLER_TTY, "w+");
    if (controllerTTY == NULL) {
        printf("Failed to open controller TTY. Error: %s\n", strerror(errno));
        return RET_OCTF;
    }
    if (piThreadCreate(controllerThread) != 0) {
        puts("Failed to create controller thread.");
        return RET_CTCF;
    }
    fprintf(CONTROLLER_TTY, "!mediaplayer %d\n", CMD_SET_VERS);   // Tell controller what command set we speak

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
            piLock(0);
            curClip = newClip;
            switchClip = false;
            piUnlock(0);
            libvlc_media_player_set_time(mp, curClip.start);
        }
        if (libvlc_media_player_get_time(mp) >= curClip.end) {
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