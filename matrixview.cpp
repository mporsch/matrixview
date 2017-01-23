#include <string>
#include <vector>
#include <algorithm>
#include <random>
#include <memory>
#include <iostream>
#include <thread>
#include <csignal>

#if defined(__linux__)
# include <sys/ioctl.h>
# include <unistd.h>
#elif defined(_WIN32)
# include "windows.h"
#else
# error platform not implemented yet
#endif

namespace constants {
  static size_t const dropletCount = 16U;
  static unsigned char const colorDecrement = 8U;

  // printable ASCII range
  static unsigned char const asciiMin = 33;
  static unsigned char const asciiMax = 126;

  static auto const frameDuration = std::chrono::milliseconds(33);
} // namespace constants

struct MatrixCharacter {
  unsigned char symbol;
  unsigned char color;

  MatrixCharacter()
    : symbol(' ')
    , color(0U) {
  }
};
using Matrix = std::vector<MatrixCharacter>;

struct Droplet {
  unsigned short x;
  unsigned short y;
};
using Droplets = std::vector<Droplet>;

struct TerminalSize {
  unsigned short width;
  unsigned short height;
};

// globally used random generator
std::default_random_engine generator;

#if defined(__linux__)

TerminalSize GetTerminalSize() {
  winsize size;
  ::ioctl(STDOUT_FILENO, TIOCGWINSZ, &size);
  return TerminalSize{size.ws_col, size.ws_row};
}

#elif defined(_WIN32)

TerminalSize GetTerminalSize() {
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
  return TerminalSize{static_cast<unsigned short>(csbi.srWindow.Right - csbi.srWindow.Left + 1),
                      static_cast<unsigned short>(csbi.srWindow.Bottom - csbi.srWindow.Top + 1)};
}

#else
# error platform not implemented yet
#endif

void ClearTerminal() {
  // CSI[2J clears screen, CSI[H moves the cursor to top-left corner
  std::cout << "\x1B[2J\x1B[H";
}

std::string HslToRgbGreen(unsigned char lightness) {
  if (lightness < 128U) {
    auto const g = static_cast<unsigned char>((lightness / 128.f) * 256U);
    auto const str = std::to_string(g);
    return "\x1B[38;2;0;" + str + ";0m";
  }
  else {
    auto const rb = static_cast<unsigned char>(((lightness - 128U) / 128.f) * 256U);
    auto const str = std::to_string(rb);
    return "\x1B[38;2;" + str + ";255;" + str + "m";
  }
}

std::unique_ptr<std::vector<std::string>> GetColorLut() {
  static size_t const size = 256;
  std::unique_ptr<std::vector<std::string>> ret(new std::vector<std::string>(size));

  for (size_t i = 0; i < size; ++i) {
    ret->at(i) = HslToRgbGreen(static_cast<unsigned char>(i));
  }

  return ret;
}

void SetTerminalColorGreen(unsigned char lightness) {
  // static lookup table (created once on startup)
  static auto const colorLut = GetColorLut();

  std::cout << colorLut->at(lightness);
}

void ResetTerminalColor() {
  std::cout << "\x1B[0m";
}

Matrix GetMatrix() {
  auto const terminalSize = GetTerminalSize();
  return Matrix(terminalSize.height * terminalSize.width);
}

void UpdateMatrix(Matrix &matrix, Droplets const &droplets) {
  auto const terminalSize = GetTerminalSize();
  std::uniform_int_distribution<> distribution(
      constants::asciiMin, constants::asciiMax);
  auto gen = [&]() -> unsigned char {
      return distribution(generator);
    };

  // handle window resize
  matrix.resize(terminalSize.height * terminalSize.width);

  // decrement all colors
  for (auto &&m : matrix) {
    if (m.color >= constants::colorDecrement) {
      m.color -= constants::colorDecrement;
    }
    else {
      m.color = 0U;
    }
  }

  for (auto &&d : droplets) {
    try {
      auto &&m = matrix.at(d.x + d.y * terminalSize.width);

      m.symbol = gen(); // update droplet symbol
      m.color = 255; // flash droplet color
    }
    catch(std::out_of_range &) {
      // ignore this droplet, until it becomes valid again
    }
  }
}

Droplets GetRandomDroplets() {
  Droplets randomDroplets(constants::dropletCount);

  auto const terminalSize = GetTerminalSize();
  std::uniform_int_distribution<unsigned short> distributionX(0, terminalSize.width);
  std::uniform_int_distribution<unsigned short> distributionY(0, terminalSize.height);
  auto gen = [&]() -> Droplet {
      return Droplet{distributionX(generator), distributionY(generator)};
    };
  std::generate(begin(randomDroplets), end(randomDroplets), gen);

  return randomDroplets;
}

void UpdateDroplets(Droplets &droplets) {
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

void Cleanup(int) {
  ClearTerminal();
  ResetTerminalColor();
  exit(EXIT_SUCCESS);
}

int main(int argc, char **argv) {
  // set up Ctrl-C handler
  if (std::signal(SIGINT, Cleanup)) {
    std::cerr << "Failed to register signal handler\n";
    return EXIT_FAILURE;
  }

  auto matrix = GetMatrix();
  auto droplets = GetRandomDroplets();

  while(true) {
    // clear and print to console
    ClearTerminal();

    for (auto &&m : matrix) {
      SetTerminalColorGreen(m.color);
      std::cout << m.symbol;
    }
    std::cout.flush();

    UpdateDroplets(droplets);
    UpdateMatrix(matrix, droplets);

    std::this_thread::sleep_for(constants::frameDuration);
  }

  return EXIT_SUCCESS;
}
