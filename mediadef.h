/***
 * 
 * The media definition for MediaPlayer
 * 
 ****/

#define MEDIA_PATH      "/home/pi/Downloads/Sequence02Low.mp4" // The video containing all the clips for the exhibit.

enum clipNames {
    idle,
    decide1,
    place1,
    place2,
    miss,
    decide2,
    CLIP_COUNT
};

enum clipTypes {
    playOnce,
    loop
};

typedef struct clip_t {
    enum clipNames clipName;        // One of the clipNames
    enum clipTypes clipType;        // Which of the clipTypes this is 
    libvlc_time_t start;            // Time clip starts (ms)
    libvlc_time_t end;              // Time clip ends (ms)
} clip_t;

// The individual clip definitions
clip_t clips[] = {
    {idle, loop, 567, 10100}, 
    {decide1, playOnce, 13300, 34667},
    {place1, playOnce, 34667, 71533},
    {place2, playOnce, 71533, 93267},
    {miss, playOnce, 93267, 106367},
    {decide2, playOnce, 106367, 119000}
};
