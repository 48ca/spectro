#include <string>
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <climits>
#include <limits>
#include <atomic>
#include <stdexcept>
#include <functional>

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

static int width = 1200;
static int height = 800;

using namespace std::string_literals;

typedef struct {
    int x;
    int width;
    int y;
    int height;
} dim_t;

class Screens {
  private:
    unsigned a_width; // array width
    unsigned a_height; // array height
  public:
    SDL_Window* window;
    SDL_Renderer* renderer;
    Screens(unsigned y = 1, unsigned x = 1) : a_width(x), a_height(y) {
        if(x == 0 || y == 0) {
            throw std::runtime_error("Tried to create an invalid screen format");
        }
        window = SDL_CreateWindow("Music", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width * x, height * y, SDL_WINDOW_OPENGL | SDL_WINDOW_BORDERLESS);
        renderer = SDL_CreateRenderer(window, -1, 0);
    }
    const dim_t get(int y, int x = 0) const { // get base and width for window dimensions
        return { x * width, width, y * height, height };
    }
};

class Display {
  private:
    std::thread thread;
  public:
    Display(std::function<void(void)> start): thread(start) {};
    ~Display() {
        thread.join();
    }
};

class Channel {
  public:
    Channel(std::vector<short>&& buf) : buffer(buf) {};
    std::vector<short> buffer;
};

class Sound { // libsndfile snd SDL_Audio abstraction
  private:
    SNDFILE* file;
    SDL_AudioSpec spec;
    SDL_AudioDeviceID dev;
    int callbackPosition = 0;
    int callbackIncrement = 0;
    int bufferSize = 0;

    static void forwardCallback(void* userdata, Uint8* stream, int len) {
        static_cast<Sound*>(userdata)->callback(reinterpret_cast<short*>(stream), len);
    }
    void callback(short* stream, int len) {
        int lenToEnd = bufferSize - callbackPosition;
        memset(stream, 0, len);
        if(!playing || lenToEnd <= 0) {
            return;
        }
        memcpy(stream, &(wave[callbackPosition]), len < lenToEnd ? len : lenToEnd);
        callbackPosition += callbackIncrement;
    }
    void setup(void) {
        logger.log("Setting up audio callback");
        SDL_AudioSpec want;
        SDL_memset(&want, 0, sizeof(want));
        want.freq = info.samplerate;
        want.format = AUDIO_S16SYS;
        want.channels = info.channels;
        want.samples = 512;
        want.callback = Sound::forwardCallback;
        want.userdata = this;
        dev = SDL_OpenAudioDevice(NULL, 0, &want, &spec, SDL_AUDIO_ALLOW_FORMAT_CHANGE);
        if(dev > 0)
            logger.log("DEVICE ID: " + std::to_string(dev));
        callbackIncrement = spec.samples * spec.channels;
        SDL_PauseAudioDevice(dev, 0);
    }
    void open(void) {
        logger.log("Opening file "s + filename.c_str());
        file = sf_open(filename.c_str(), SFM_READ, &info);
        if(!file) {
            logger.error("libsndfile error occurred on read (unrecognized format?)");
            failedToOpen = true;
        }
    }

    void fillBuffer(void) {
        wave.resize(info.frames * info.channels);
        bufferSize = wave.size();
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
    bool playing = false;

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

    int getCallbackPosition() {
        return callbackPosition;
    }

    bool startPlaying(unsigned sample) {
        int position = sample * info.channels;
        int len = sizeof(short) * (wave.size() - position);
        if(len <= 0) {
            logger.error("Attempted to start playing past end of song");
            return false;
        }
        callbackPosition = position;
        playing = true;
        return true;
    }

    Sound(std::string fn) : file(nullptr), filename(fn) {
        open();
        setup();
    }
    ~Sound() {
        if(file != nullptr)
            sf_close(file);
        if(dev > 0)
            SDL_CloseAudioDevice(dev);
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

void exit(void) {
    logger.log("Exiting...");
    SDL_Quit();
}

int main(int argc, char** argv) {
    auto args = parseArgs(argc, argv);
    if(args.exitCode) {
        return 1;
    }

    logger.log("Initializing SDL");
    if( SDL_Init( SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_AUDIO ) == -1 ) {
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

    bool running = true;
    std::thread sound_thread([&running, &snd]() {
        logger.log("Started sound thread");
        /*
        snd.startPlaying(3.5 * 60 * snd.info.samplerate); // start 3.5 minutes into the song
        */
        snd.startPlaying(0);
        while(running) {
            SDL_Delay(30);
        }
    });

    constexpr int fftwidth = 3000;

    Screens scrs(snd.info.channels, 2);

    auto s = std::chrono::high_resolution_clock::now();

    fftw_complex *in  = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * fftwidth);
    fftw_complex *out = (fftw_complex*) fftw_malloc(sizeof(fftw_complex) * fftwidth);
    fftw_plan p;
    p = fftw_plan_dft_1d(fftwidth, in, out, FFTW_FORWARD, FFTW_ESTIMATE);

    std::atomic<unsigned> frames(0);
    std::thread display_thread([&]() {
    // std::thread display_thread([&running, &snd, &s, &frames, &scrs]() {

        auto displayScope = [&scrs](unsigned scr, const std::vector<short>& buffer, int below, int size) {
            auto d = scrs.get(scr, 0); // x, width, y, height
            SDL_SetRenderDrawColor(scrs.renderer, 255, 255, 255, 255);
            for(int i = 1; i < size; ++i) {
                SDL_RenderDrawLine(scrs.renderer, d.x + i - 1, d.y + d.height/2 - (d.height*buffer[below + i - 1]/SHRT_MAX)/2,
                        d.x + i, d.y + d.height/2 - (d.height*buffer[below + i]/SHRT_MAX)/2);
            }
        };

        auto displayFFT = [&scrs, &in, &out, &p](unsigned scr, const std::vector<short>& buffer, int below) {
            for(int i = 0; i < fftwidth; ++i) {
                in[i][0] = (double)(buffer[i + below])/SHRT_MAX;
                in[i][1] = 0;
            }
            fftw_execute(p);
            for(int i = 0; i < fftwidth/2; ++i) {
                out[i][0] = (out[i][0]*out[i][0]+out[i][1]*out[i][1]) * 2.0/(fftwidth); // divide by sum(window)
                out[i][0] = 20 * log10(out[i][0]); // db
                if(out[i][0] < -80) out[i][0] = -80;
                out[i][0] /= 80;
                out[i][0] = out[i][0] + 1;
            }
            /*
            for(int i = 0; i < fftwidth/2; ++i) {
                std::cout << out[i][0] << ' ';
            }
            std::cout << '\n';
            */
            auto d = scrs.get(scr, 1); // x, width, y, height
            for(int i = 0; i < d.width; ++i) {
                double s = out[i * fftwidth/(d.width * 2)][0];
                // SDL_SetRenderDrawColor(scrs.renderer, 255, 255 * s, 255 * s, 255);
                SDL_RenderDrawLine(scrs.renderer, d.x + i, d.y + d.height * (1-s/2),
                        d.x + i, d.y + d.height);
            }
        };

        logger.log("Starting display loop");

        int oldsample = -1;
        while(running) {
            SDL_SetRenderDrawColor(scrs.renderer, 0, 0, 0, 0);
            SDL_RenderClear(scrs.renderer);
            /*
            auto n = std::chrono::high_resolution_clock::now();
            auto d = std::chrono::duration<double>(n - s); // where in the samples to use
            int sample = d.count() * snd.info.samplerate;
            */
            int sample = snd.getCallbackPosition()/snd.info.channels;
            if(sample == oldsample) {
                SDL_Delay(10);
                continue;
            }
            if(sample >= snd.info.frames - width) running = false;
            for(int i = 0; i < snd.info.channels; ++i) {
                // auto dwidth = std::min(snd.channels[i].buffer.size() - sample-width/2, width);
                auto dwidth = width;
                displayScope(i, snd.channels[i].buffer, std::max(sample - width/2, 0), dwidth);
                displayFFT(i, snd.channels[i].buffer, std::max(sample - fftwidth/2, 0));
            }

            SDL_RenderPresent(scrs.renderer);
            /*
            logger.log("Running FFT at sample: " + std::to_string(sample));
            displayFFT(screen, snd.channels[0].buffer, below, samplewidth);
            SDL_Delay(1);
            */
            oldsample = sample;
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
                    if(event.key.keysym.sym == SDLK_q)
                        running = false;
                    if(event.key.keysym.sym == SDLK_SPACE)
                        snd.playing = !snd.playing;
                    break;
            }
        }
    }

    fftw_free(in);
    fftw_free(out);

    exit();
    sound_thread.join();
    display_thread.join();
    fps_thread.join();

    return 0;
}
