#include <string>
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>

#include <stdlib.h>
#include <sndfile.hh>

#include <fftw3.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_surface.h>
#include <SDL2/SDL.h>

static SDL_Window* screen;

struct arguments {
    std::string filename;
    short exitCode = 0;
    std::string callee;
};

class Sound { // libsndfile snd SDL_mixer abstraction
  private:
    SNDFILE* file;

    void open(void) {
        std::cout << "Opening file..." << std::endl;
        file = sf_open(filename.c_str(), SFM_READ, &info);
        std::cout << "Reading file" <<  std::endl;
        if(!file) {
            std::cerr << "libsndfile error occurred on read (unrecognized format?)" << std::endl;
            failedToOpen = true;
        }
        std::cout << "Reading file again" <<  std::endl;
        music = Mix_LoadMUS(filename.c_str());
        if(!music) {
            std::cerr << "Mix error: " << Mix_GetError() << std::endl;
            failedToOpen = true;
        }
    }

    void fillBuffer(void) {
        if(!buffer.empty()) {
            std::cerr << "Buffer already full?" << std::endl;
        }
        buffer.reserve(info.frames * info.channels);
        /*sf_count_t read = */
        sf_readf_double(file, &buffer[0], info.frames);
    }

  public:
    Mix_Music *music;
    std::string filename;
    SF_INFO info;
    std::vector<double> buffer;
    bool failedToOpen = false;

    void displayInfo(void) const {
        std::cout << "/***** " << filename << " *****\\" << '\n'
                  << " * Channels: " << info.channels << '\n'
                  << " * Frames: " << info.frames << '\n'
                  << " * Sample rate: " << info.samplerate << '\n'
                  << "\\******" << std::string(filename.length(), '*') << "******/" << std::endl;
    }

    void read(void) {
        if(file)
            fillBuffer();
    }

    Sound(std::string fn) : file(nullptr), filename(fn) {
        open();
    }
    ~Sound() {
        if(file != nullptr)
            sf_close(file);
        if(music != nullptr)
            Mix_FreeMusic(music);
    }
};

void displayFFT(const std::vector<double>& buffer) {
    if(!screen) return;
}

struct arguments parseArgs(int argc, char** argv) {
    struct arguments args;
    args.callee = argv[0];
    for(int i = 1;  i < argc; ++i) {
        std::string arg = argv[i]; // make this real
        if(args.filename.length()) {
            std::cerr << "Tried to read multiple files" << std::endl;
            args.exitCode = 1;
            break;
        }
        args.filename = std::move(arg);
    }
    if(args.filename.empty()) args.exitCode = 1;
    return args;
}

void display(const Sound& sound) {
    if(sound.info.channels != 2) {
        std::cout << "Stereo input only please" << std::endl;
        return;
    }
    std::cout << "Displaying..." << std::endl;

    const long sample_size = 44100;
    unsigned long max = sound.info.frames * sound.info.channels;
    unsigned long left = 0;
    unsigned long right = 1;
    for(;
            left < max && right < max;
            left += sample_size * 2, right += sample_size * 2) {
    }
}

void play(const Sound& sound) {
    if(!sound.music) {
        std::cerr << "Failed to play music with SDL" << std::endl;
    }
    std::cout << "Starting playback" << std::endl;
    if(Mix_PlayMusic(sound.music, 1) == -1) {
        std::cerr << Mix_GetError() << std::endl;
    }
}

bool init()
{
    //Initialize all SDL subsystems
    if( SDL_Init( SDL_INIT_AUDIO | SDL_INIT_VIDEO ) == -1 )
    {
        return false;
    }

    /*
    //Set up the screen
    screen = SDL_SetVideoMode( SCREEN_WIDTH, SCREEN_HEIGHT, SCREEN_BPP, SDL_SWSURFACE );

    //If there was an error in setting up the screen
    if( screen == NULL )
    {
        return false;
    }
    */

    screen = SDL_CreateWindow("Music",
                          SDL_WINDOWPOS_UNDEFINED,
                          SDL_WINDOWPOS_UNDEFINED,
                          640, 480, SDL_WINDOW_OPENGL);

    //Initialize SDL_mixer
    if( Mix_OpenAudio( 22050, MIX_DEFAULT_FORMAT, 2, 4096 ) == -1 )
    {
        return false;
    }

    //If everything initialized fine
    return true;
}

void exit(void) {
    std::cout << "Exiting" << std::endl;
    SDL_Quit();
}

int main(int argc, char** argv) {
    auto args = parseArgs(argc, argv);
    if(args.exitCode) {
        return 1;
    }
    std::cout << "Reading: ";
    std::cout << args.filename;
    std::cout << std::endl;

    if(!init()) {
        std::cerr << "Initialization is fucked" << std::endl;
        return 1;
    }

    if(!Mix_Init(MIX_INIT_FLAC | MIX_INIT_MOD | MIX_INIT_MP3 | MIX_INIT_OGG)) {
        std::cerr << "Mix_Init error: " << Mix_GetError() << std::endl;
        return 1;
    }

    Sound snd(args.filename);
    if(snd.failedToOpen) {
        std::cout << "Failed to open the file" << std::endl;
        return 1;
    }
    snd.displayInfo();
    snd.read();
    std::cout << "Starting playing" << std::endl;
    play(snd);

    std::atexit(exit);

    while(true) {
        SDL_Event event;
        while(SDL_PollEvent(&event)) {
            switch(event.type) {
                case SDL_KEYDOWN:
                    return 0; // calls std::atexit
                default:
                    break;
            }
        }
    }

    return 0;
}
