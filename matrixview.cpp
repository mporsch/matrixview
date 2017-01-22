#include <vector>
#include <algorithm>
#include <random>
#include <iostream>
#include <thread>

#ifdef __linux__
# include <sys/ioctl.h>
# include <unistd.h>
#elif _WIN32
# error platform not implemented yet
#else
# error platform not supported
#endif

namespace constants {
  static size_t const dropletCount = 12;

  // printable ASCII range
  static int const asciiMin = 32;
  static int const asciiMax = 126;

  static auto const frameDuration = std::chrono::milliseconds(50);
} // namespace constants

std::default_random_engine generator;

struct Droplet {
  unsigned short x;
  unsigned short y;
};

winsize getTerminalSize() {
  winsize size;
  ::ioctl(STDOUT_FILENO, TIOCGWINSZ, &size);
  return size;
}

std::vector<Droplet> getRandomDroplets() {
  std::vector<Droplet> randomDroplets(constants::dropletCount);

  auto const terminalSize = getTerminalSize();
  std::uniform_int_distribution<unsigned short> distributionX(0, terminalSize.ws_col);
  std::uniform_int_distribution<unsigned short> distributionY(0, terminalSize.ws_row);
  auto gen = [&]() -> Droplet {
      return Droplet{distributionX(generator), distributionY(generator)};
    };
  std::generate(begin(randomDroplets), end(randomDroplets), gen);

  return randomDroplets;
}

void updateDroplets(std::vector<Droplet> &droplets) {
  auto const terminalSize = getTerminalSize();
  std::uniform_int_distribution<unsigned short> distributionX(0, terminalSize.ws_col);
  auto gen = [&]() -> Droplet {
      return Droplet{distributionX(generator), 0};
    };

  for (auto &&d : droplets) {
    ++d.y;
    if (d.y > terminalSize.ws_row) {
      d = gen();
    }
  }
}

std::vector<unsigned char> getBuffer() {
  auto const terminalSize = getTerminalSize();

  std::vector<unsigned char> randomBuffer(terminalSize.ws_row * terminalSize.ws_col, ' ');
  return randomBuffer;
}

void updateBuffer(std::vector<unsigned char> &buffer, std::vector<Droplet> const &droplets) {
  auto const terminalSize = getTerminalSize();
  std::uniform_int_distribution<unsigned char> distribution(
      constants::asciiMin, constants::asciiMax);
  auto gen = [&]() -> unsigned char {
      return distribution(generator);
    };

  buffer.resize(terminalSize.ws_row * terminalSize.ws_col, ' ');
  for (auto &&d : droplets) {
    try {
      buffer.at(d.x + d.y * terminalSize.ws_col) = gen();
    }
    catch(std::out_of_range &) {
      // ignore this droplet, until it becomes valid again
    }
  }
}

void clear() {
  // CSI[2J clears screen, CSI[H moves the cursor to top-left corner
  std::cout << "\x1B[2J\x1B[H";
}

int main(int argc, char **argv) {
  auto buffer = getBuffer();
  auto droplets = getRandomDroplets();

  while(true) {
    // clear and print to console
    clear();
    std::cout.write(
        reinterpret_cast<char const *>(buffer.data()),
        buffer.size());
    std::cout.flush();

    updateDroplets(droplets);
    updateBuffer(buffer, droplets);

    std::this_thread::sleep_for(constants::frameDuration);
  }
}
