#include <linked_nodes/languages.hpp>
#include <iostream>
#include <fstream>
#include <thread>
#include <unistd.h>
#include <signal.h>
#include <poll.h>
#include <sys/socket.h>

struct fps_display {
  float current_sum{0}, avg{0};;
  unsigned int count{0}, loop;

  fps_display(unsigned int loop) : loop(loop) {}
  float feed(double value) {
    current_sum += value;
    count++;
    if (count == loop) {
      avg = current_sum / loop;
      count = 0;
      current_sum = 0.0;
    }
    return avg;
  }
};

FILE* lemonbar;

void sighandle(int signal) {
  if (signal == SIGINT || signal == SIGTERM)
    exit(0);
}

void cleanup() {
  if (lemonbar) {
    fclose(lemonbar);
    lemonbar = nullptr;
  }
}

int main(int argc, char** argv) {
  std::ifstream file {"lemonbar.yml"};
  if (file.fail()) {
    std::cout << "Failed to load file 'lemonbar.yml'. "
        "Make sure that you are working in the directory test/example." << std::endl;
    return 1;
  }
  node::errorlist err;
  auto wrapper = parse_yml(file, err);
  file.close();

  if (!err.empty()) {
    std::cerr << "Error while parsing:\n";
    for (auto& e : err)
      std::cerr << "At " << e.first << ": " << e.second << std::endl;
    return -1;
  }
  node::clone_context context;
  wrapper->optimize(context);
  if (!context.errors.empty()) {
    std::cerr << "Error while optimizing:\n";
    for (auto& e : context.errors)
      std::cerr << "At " << e.first << ": " << e.second << std::endl;
    return -1;
  }

  // Fork and call lemonbar
  atexit(cleanup);
  signal(SIGINT, sighandle);
  signal(SIGINT, sighandle);
  int pipes[2];
  if (socketpair(AF_UNIX, SOCK_STREAM, 0, pipes) < 0)
    throw std::runtime_error("socketpair failed");
  switch(auto pid = fork()) {
    case -1:
      close(pipes[0]);
      close(pipes[1]);
      throw std::runtime_error("fork failed");
    case 0: // Child case
      dup2(pipes[1], STDOUT_FILENO);
      dup2(pipes[1], STDIN_FILENO);
      close(pipes[0]);
      close(pipes[1]);
      execl("/usr/bin/sh", "sh", "-c", argc > 1 ? argv[1] : "lemonbar", nullptr);
      return 127;
  }
  // Parent case
  lemonbar = fdopen(pipes[0], "r+");
  close(pipes[1]);
  pollfd pollin{pipes[0], POLLIN, 0};

  fps_display fps(40);
  auto node = wrapper->get_child_ptr("lemonbar"_ts);
  if (!node)
    std::cerr << "Failed to retrieve the key at path 'lemonbar'";
  while (true) {
    if (poll(&pollin, 1, 0) > 0 && pollin.revents & POLLIN) {
      char buffer[128];
      char* buffer_end = buffer;
      while (true) {
        auto count = read(pipes[0], buffer, 128);
        if (count == 0) break;
        if (count < 0) return -1;
        buffer_end += count;
        char* line_start = buffer, *newline;
        while ((newline = (char*)memchr(line_start, '\n', buffer_end - line_start))) {
          *newline = 0;
          if (!memcmp(line_start, "save ", 5)) {
            line_start += 5;
            for (; line_start < newline; line_start++)
              if (!std::isspace(*line_start))
                break;
            auto sep = (char*)memchr(line_start, '=', buffer_end - line_start);
            if (sep) {
              *sep = 0;
              if (!wrapper->set<string>(tstring(line_start), string(sep+1)))
                std::cerr << "Can't set key: " << line_start << std::endl;
            } else std::cerr << "Invalid: " << line_start << std::endl;
          } else {
            system(line_start);
          }
          line_start = newline + 1;
        }
        if (count < 128 || buffer[127] == '\n') break;
      }
    }

    auto now = std::chrono::steady_clock::now();
    try {
      auto result = node->get();
      auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - now).count() / 1000000.0;
      if (!wrapper->set<float>("lemonbar.frame.calculation"_ts, round(10*fps.feed(elapsed))/10))
        std::cerr << "Can't set frame time" << std::endl;
      fputs(result.data(), lemonbar);
      fflush(lemonbar);
    } catch (const std::exception& e) {
      std::cerr << "Error while retrieving key: " << e.what() << std::endl;
    }
    std::this_thread::sleep_until(now + std::chrono::milliseconds(50));
  }
}
