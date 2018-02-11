#include <string>
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>

#include "logger.h"

#include <stdlib.h>
#include <sndfile.hh>

#include <fftw3.h>
#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_surface.h>
#include <SDL2/SDL.h>

static SDL_Window* screen;

static Logger logger;

struct arguments {
    std::string filename;
    short exitCode = 0;
    std::string callee;
};

using namespace std::string_literals;

class Channel {
  public:
    Channel(std::vector<short>&& buf) : buffer(buf) {};
    std::vector<short> buffer;
};

class Sound { // libsndfile snd SDL_mixer abstraction
  private:
    SNDFILE* file;

    void open(void) {
        logger.log("Opening file "s + filename.c_str());
        file = sf_open(filename.c_str(), SFM_READ, &info);
        if(!file) {
            logger.error("libsndfile error occurred on read (unrecognized format?)");
            failedToOpen = true;
        }
        logger.log("Opening audio PCM 16-bit "s + std::to_string(info.samplerate) + "Hz " + std::to_string(info.channels) + " channels");
        if( Mix_OpenAudio( info.samplerate , AUDIO_S16SYS, info.channels, 4096 ) == -1 )
            failedToOpen = false;
    }

    void fillBuffer(void) {
        wave.resize(info.frames * info.channels);
        sf_count_t read = sf_readf_short(file, &wave[0], info.frames); // short
        logger.log("Read " + std::to_string(read) + " frames");
        for(int i = 0; i < info.channels; ++i) {
            std::vector<short> tbuf(info.frames);
            for(unsigned s = 0; s < info.frames; ++s)
                tbuf[s] = wave[s * info.channels + i];
            channels.emplace_back(std::move(tbuf));
        }
        logger.log("Populated "s + std::to_string(info.channels) + " channels");
    }

  public:
    std::string filename;
    SF_INFO info = {};
    std::vector<Channel> channels;
    std::vector<short> wave; // libsndfile
    // std::vector<short> buffer;
    bool failedToOpen = false;

    std::string getHumanLength() const {
        if(!info.frames) return "ERROR";
        int tseconds = info.frames/info.samplerate;
        int minutes = tseconds/60;
        int seconds = tseconds%60;
        std::string secstr  = std::to_string(seconds);
        return std::to_string(minutes)+':' + std::string(2 - secstr.length(), '0') + secstr;
    }
    void displayInfo(void) const {
        std::cout << "/***** " << filename << " *****\\" << '\n'
                  << " * Channels: " << info.channels << '\n'
                  << " * Frames: " << info.frames << '\n'
                  << " * Sample rate: " << info.samplerate << '\n'
                  << " * Length: " << getHumanLength() << '\n'
                  << "\\******" << std::string(filename.length(), '*') << "******/" << std::endl;
    }

    void read(void) {
        if(file)
            fillBuffer();
    }

    bool startPlaying(unsigned sample) {
        int position = sample * info.channels;
        int len = sizeof(short) * (wave.size() - position);
        if(len <= 0) {
            logger.error("Attempted to start playing past end of song");
            return false;
        }
        Mix_Chunk chnk;
        chnk.allocated = 1;
        chnk.abuf = (uint8_t*) &(wave[position]);
        chnk.alen = sizeof(short) * (wave.size() - position);
        chnk.volume = 100; // 128 = MAX
        if(Mix_PlayChannel(-1, &chnk, 0) == -1) {
            logger.error("Error in Mix_PlayChannel: "s + Mix_GetError());
            return false;
        }
        return true;
    }

    Sound(std::string fn) : file(nullptr), filename(fn) {
        open();
    }
    ~Sound() {
        if(file != nullptr)
            sf_close(file);
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
        return;
    }

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
    if( SDL_Init( SDL_INIT_AUDIO | SDL_INIT_VIDEO ) == -1 )
    {
        return false;
    }

    screen = SDL_CreateWindow("Music",
                          SDL_WINDOWPOS_UNDEFINED,
                          SDL_WINDOWPOS_UNDEFINED,
                          640, 480, SDL_WINDOW_OPENGL);

    return true;
}

void exit(void) {
    logger.log("Exiting...");
    Mix_CloseAudio();
    Mix_Quit();
    SDL_Quit();
}

int main(int argc, char** argv) {
    auto args = parseArgs(argc, argv);
    if(args.exitCode) {
        return 1;
    }

    if(!init()) {
        logger.error("Failed to initialize");
        return 1;
    }

    Sound snd(args.filename);
    if(snd.failedToOpen) {
        logger.error("Failed to open file");
        return 1;
    }
    snd.displayInfo();
    snd.read();

    // Mix_AllocateChannels(2);
    // Mix_SetPanning(1, 255, 0);
    // Mix_SetPanning(2, 0, 255);

    std::thread sound_thread([&snd]() {
        logger.log("Started sound thread");
        /*
        snd.startPlaying(3.5 * 60 * snd.info.samplerate); // start 3.5 minutes into the song
        Mix_HaltChannel(-1);
        */
        snd.startPlaying(0);
    });

    std::thread display_thread([]() {

    });

    SDL_Event event;
    bool running = true;
    while(running) {
        while(SDL_PollEvent(&event)) {
            logger.log("got event");
            switch(event.type) {
                case SDL_KEYDOWN:
                    running = false;
                    break;
            }
            // displayFFT(snd);
        }
    }

    exit();
    sound_thread.join();
    display_thread.join();

    return 0;
}
