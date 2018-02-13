#include <string>
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <climits>
#include <atomic>

#include "logger.h"

#include <stdlib.h>
#include <sndfile.hh>

#include <complex.h> // for fftw_complex == double complex
#include <fftw3.h>

#include <SDL2/SDL_mixer.h>
#include <SDL2/SDL_surface.h>
#include <SDL2/SDL.h>

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

void displayFFT(const SDL_Window* scr, const std::vector<short>& buffer, int below, int size) {
    fftw_complex *in, *out;
    fftw_plan p;

    in = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * size);
    out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * size);
    p = fftw_plan_dft_1d(size, in, out, FFTW_FORWARD, FFTW_ESTIMATE);
    for(int i = 0; i < size; ++i) {
        in[i][0] = buffer[i + below];
        in[i][1] = 0;
    }
    fftw_execute(p);
    for(int i = 0; i < size; ++i) {
        out[i][0] = out[i][0]/size;
        out[i][1] = out[i][1]/size;
    }
    for(int i = 0; i < size; ++i) {
        std::cout << sqrt(out[i][0]*out[i][0] + out[i][1]*out[i][1]) << ' ';
    }
    std::cout << '\n';
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

    logger.log("Initializing SDL");
    if( SDL_Init( SDL_INIT_AUDIO | SDL_INIT_VIDEO ) == -1 ) {
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
    Mix_AllocateChannels(1);

    bool running = true;
    std::thread sound_thread([&running, &snd]() {
        logger.log("Started sound thread");
        /*
        snd.startPlaying(3.5 * 60 * snd.info.samplerate); // start 3.5 minutes into the song
        Mix_HaltChannel(-1);
        */
        snd.startPlaying(0);
        while(running) {
            SDL_Delay(100);
        }
    });

    auto s = std::chrono::high_resolution_clock::now();

    std::atomic<unsigned> frames(0);
    std::thread display_thread([&running, &snd, &s, &frames]() {
        const int samplewidth = 10;

        int width = 1280;
        int height = 1024;

        SDL_Window* screen = SDL_CreateWindow("Music",
                              SDL_WINDOWPOS_UNDEFINED,
                              SDL_WINDOWPOS_UNDEFINED,
                              width, height, SDL_WINDOW_OPENGL);

        SDL_Renderer* renderer = SDL_CreateRenderer(screen, -1, 0);

        auto displayScope = [&width, &height](SDL_Renderer* const renderer, const std::vector<short>& buffer, int below, int size) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            for(unsigned i = 0; i < size; ++i) {
                SDL_RenderDrawPoint(renderer, i, height/2 - (height*buffer[below + i]/SHRT_MAX)/5);
            }
        };

        while(running) {
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255); //red
            SDL_RenderClear(renderer);
            auto n = std::chrono::high_resolution_clock::now();
            auto d = std::chrono::duration<double>(n - s); // where in the samples to use
            int sample = d.count() * snd.info.samplerate;
            int below = sample - samplewidth/2;
            displayScope(renderer, snd.channels[0].buffer, std::max(sample - width/2, 0), width);
            SDL_RenderPresent(renderer);
            /*
            logger.log("Running FFT at sample: " + std::to_string(sample));
            displayFFT(screen, snd.channels[0].buffer, below, samplewidth);
            SDL_Delay(1);
            */
            frames.fetch_add(1, std::memory_order_relaxed);
        }
    });

    std::thread fps_thread([&running, &frames]() {
        while(running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            unsigned fps = frames.exchange(0, std::memory_order_relaxed);
            logger.log("FPS: "s  + std::to_string(fps));
        }
    });

    SDL_Event event;
    while(running) {
        while(SDL_PollEvent(&event)) {
            logger.log("got event");
            switch(event.type) {
                case SDL_KEYDOWN:
                    running = false;
                    break;
            }
        }
    }

    exit();
    sound_thread.join();
    display_thread.join();
    fps_thread.join();

    return 0;
}
