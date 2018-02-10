#include <string>
#include <iostream>
#include <vector>

#include <stdlib.h>
#include <sndfile.hh>

#include <fftw3.h>
#include <SDL2/SDL_mixer.h>

struct arguments {
    std::string filename;
    short exitCode = 0;
    std::string callee;
};

class Sound { // libsndfile abstraction
  private:
    SNDFILE* file;

    void open(void) {
        std::cout << "Opening file..." << std::endl;
        file = sf_open(filename.c_str(), SFM_READ, &info);
        if(!file) failedToOpen = true;
        music = Mix_LoadMUS(filename.c_str());
        if(!music) failedToOpen = true;
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
    }
};

void displayFFT(const std::vector<double>& buffer) {

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
}

int main(int argc, char** argv) {
    auto args = parseArgs(argc, argv);
    if(args.exitCode) {
        return 1;
    }
    std::cout << "Reading: ";
    std::cout << args.filename;
    std::cout << std::endl;

    {
        Sound snd(args.filename);
        if(snd.failedToOpen)
            std::cout << "Failed to open the file" << std::endl;
        else {
            snd.displayInfo();
            snd.read();
            play(snd);
            display(snd);
        }
    }

    return 0;
}
