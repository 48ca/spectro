#include <string>
#include <iostream>

#include <stdlib.h>

#include <sndfile.hh>

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
    }

    void fillBuffer(void) {
        if(buffer) {
            std::cerr << "Buffer already full?" << std::endl;
            return;
        }
        std::cout << "Allocating..." << std::endl;
        buffer = (double*) malloc(sizeof(double) * info.frames * info.channels);
        std::cout << "Reading " << info.frames << " frames..." << std::endl;
        sf_count_t read = sf_readf_double(file, buffer, info.frames);
        std::cout << "Read " << read << " frames" << std::endl;
    }

  public:
    std::string filename;
    SF_INFO info;
    double* buffer = nullptr;

    void displayInfo(void) const {
        std::cout << "/***** SOUND FILE *****/" << '\n'
                  << " * Filename: " << filename << '\n'
                  << " * Channels: " << info.channels << '\n'
                  << " * Frames: " << info.frames << '\n'
                  << " * Sample rate: " << info.samplerate << '\n'
                  << "/**********************/" << std::endl;
    }

    void read(void) {
        fillBuffer();
    }

    Sound(std::string fn) : file(nullptr), filename(fn) {
        open();
    }
    ~Sound() noexcept {
        if(file != nullptr)
            sf_close(file);
        if(buffer != nullptr) {
            free(buffer);
        }
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
};

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
        if(!snd.info.frames)
            std::cout << "Failed to open the file" << std::endl;
        else {
            snd.displayInfo();
            snd.read();
        }
    }

    return 0;
}
