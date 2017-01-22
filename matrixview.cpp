#include <vector>
#include <algorithm>
#include <random>
#include <iostream>
#include <thread>

#ifdef __linux__
# include <sys/ioctl.h>
# include <unistd.h>
#else
# error platform not implemented yet
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

struct TerminalSize {
  unsigned short width;
  unsigned short height;
};

namespace unix {
  TerminalSize GetTerminalSize() {
    winsize size;
    ::ioctl(STDOUT_FILENO, TIOCGWINSZ, &size);
    return TerminalSize{size.ws_col, size.ws_row};
  }

  void ClearTerminal() {
    // CSI[2J clears screen, CSI[H moves the cursor to top-left corner
    std::cout << "\x1B[2J\x1B[H";
  }
} // namespace unix

#ifdef __linux__
  using namespace unix;
#else
# error platform not implemented yet
#endif

std::vector<Droplet> GetRandomDroplets() {
  std::vector<Droplet> randomDroplets(constants::dropletCount);

  auto const terminalSize = GetTerminalSize();
  std::uniform_int_distribution<unsigned short> distributionX(0, terminalSize.width);
  std::uniform_int_distribution<unsigned short> distributionY(0, terminalSize.height);
  auto gen = [&]() -> Droplet {
      return Droplet{distributionX(generator), distributionY(generator)};
    };
  std::generate(begin(randomDroplets), end(randomDroplets), gen);

  return randomDroplets;
}

void UpdateDroplets(std::vector<Droplet> &droplets) {
  auto const terminalSize = GetTerminalSize();
  std::uniform_int_distribution<unsigned short> distributionX(0, terminalSize.width);
  auto gen = [&]() -> Droplet {
      return Droplet{distributionX(generator), 0};
    };

  for (auto &&d : droplets) {
    // move droplet downward
    ++d.y;
    if (d.y > terminalSize.height) {
      // recreate droplet at the top
      d = gen();
    }
  }
}

std::vector<unsigned char> GetBuffer() {
  auto const terminalSize = GetTerminalSize();

  std::vector<unsigned char> randomBuffer(terminalSize.height * terminalSize.width, ' ');
  return randomBuffer;
}

void UpdateBuffer(std::vector<unsigned char> &buffer, std::vector<Droplet> const &droplets) {
  auto const terminalSize = GetTerminalSize();
  std::uniform_int_distribution<unsigned char> distribution(
      constants::asciiMin, constants::asciiMax);
  auto gen = [&]() -> unsigned char {
      return distribution(generator);
    };

  buffer.resize(terminalSize.height * terminalSize.width, ' ');
  for (auto &&d : droplets) {
    try {
      buffer.at(d.x + d.y * terminalSize.width) = gen();
    }
    catch(std::out_of_range &) {
      // ignore this droplet, until it becomes valid again
    }
  }
}

int main(int argc, char **argv) {
  auto buffer = GetBuffer();
  auto droplets = GetRandomDroplets();

  while(true) {
    // clear and print to console
    ClearTerminal();
    std::cout.write(
        reinterpret_cast<char const *>(buffer.data()),
        buffer.size());
    std::cout.flush();

    UpdateDroplets(droplets);
    UpdateBuffer(buffer, droplets);

    std::this_thread::sleep_for(constants::frameDuration);
  }
}
