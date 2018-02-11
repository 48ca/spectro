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

class Channel {
  public:
    Channel(std::vector<short>&& buf) : buffer(buf) {};
    std::vector<short> buffer;
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
    }

    void fillBuffer(void) {
        std::vector<short> buffer(info.frames * info.channels);
        buffer.reserve(info.frames * info.channels);
        /*sf_count_t read = */
        sf_readf_short(file, (short*)&buffer[0], info.frames); // short
        std::cout << "Read" << std::endl;
        for(int i = 0; i < info.channels; ++i) {
            std::vector<short> tbuf(info.frames);
            for(unsigned s = 0; s < info.frames; ++s)
                tbuf[s] = buffer[s * info.channels + i];
            channels.emplace_back(std::move(tbuf));
        }
        std::cout << "Created channel[0] with " << channels[0].buffer.size() << " entries" << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << std::endl;
    }

  public:
    Mix_Music *music;
    std::string filename;
    SF_INFO info;
    std::vector<Channel> channels;
    // std::vector<short> buffer;
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

    bool startPlaying(unsigned position) {
        /*
        for(unsigned i = 10000; i < 10100; ++i) {
            std::cout << channels[0].buffer[i] << ' ';
        }
        std::cout << std::endl;
        */
        Mix_Chunk chnk;
        chnk.allocated = 1;
        chnk.abuf = (uint8_t*) &(channels[0].buffer[position]);
        chnk.alen = channels[0].buffer.size() - position;
        chnk.volume = 128;
        if(Mix_PlayChannel(-1, &chnk, 1) == -1) {
            std::cout << Mix_GetError() << std::endl;
            return false;
        }
        return true;
        Mix_Chunk* c;
        c = Mix_LoadWAV(filename.c_str());
        if(Mix_PlayChannel(-1, c, 1) == -1) {
            std::cout << Mix_GetError() << std::endl;
            return false;
        }
        std::cout << "Playing" << std::endl;
        return true;
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

void displayFFT(const Sound& sound) {
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
    if( Mix_OpenAudio( 22050, AUDIO_S16SYS, 2, 4096 ) == -1 )
    {
        return false;
    }

    //If everything initialized fine
    return true;
}

void exit(void) {
    std::cout << "Exiting" << std::endl;
    Mix_Quit();
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

    if(!Mix_Init(MIX_INIT_FLAC | MIX_INIT_MOD | MIX_INIT_MP3 | MIX_INIT_OGG)) { // use cmake to figure out which of these should we used
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

    std::atexit(exit);

    if(snd.startPlaying(1000000))
        while(1);
        // std::this_thread::sleep_for(std::chrono::seconds(10));

    SDL_Event event;
    while(SDL_PollEvent(&event)) {
        switch(event.type) {
            case SDL_KEYDOWN:
                return 0; // calls std::atexit
            default:
                break;
        }
        displayFFT(snd);
    }

    return 0;
}
