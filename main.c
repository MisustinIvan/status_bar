#include <X11/Xlib.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

void draw_bar();

#define NSECTIONS 4
#define SECTION_SIZE 64
#define BAR_BUFFER_SIZE (NSECTIONS * SECTION_SIZE) + 1
static char bar_buffer[BAR_BUFFER_SIZE];
static Display *display;
static Window root;
static pid_t pids[NSECTIONS];

typedef struct Section {
  int timeout;
  char text[SECTION_SIZE];
  void (*updater)(struct Section *);
} Section;

static Section *sections;

void section_loop(struct Section *section) {
  while (1) {
    section->updater(section);
    draw_bar();
    sleep(section->timeout);
  }
}

pid_t section_go(struct Section *section) {
  pid_t pid;
  pid = fork();

  if (pid == 0) {
    section_loop(section);
  } else if (pid < 0) {
    perror("Failed to fork");
    exit(EXIT_FAILURE);
  }

  return pid;
}

void draw_bar() {
  for (int i = 0; i < NSECTIONS; i++) {
    strcat(bar_buffer, sections[i].text);
  }
  strcat(bar_buffer, "\0");

  XStoreName(display, root, bar_buffer);
  XFlush(display);
  memset(bar_buffer, 0, sizeof(bar_buffer));
}

void player_section_updater(struct Section *section) {
  FILE *fp = popen("playerctl metadata title", "r");
  if (fp == NULL) {
    snprintf(section->text, SECTION_SIZE, "[Error]");
    return;
  }

  char buffer[SECTION_SIZE - 3];
  size_t n = fread(buffer, 1, SECTION_SIZE - 3, fp);
  buffer[n] = '\0';
  pclose(fp);

  char *newline = strchr(buffer, '\n');
  if (newline) {
    *newline = '\0';
  }

  snprintf(section->text, SECTION_SIZE, "[%s]", buffer);
}

void time_section_updater(struct Section *section) {
  time_t now = time(NULL);
  struct tm *tm_info = localtime(&now);
  strftime(section->text, SECTION_SIZE, "[%d. %m. %Y - %H:%M:%S]", tm_info);
}

void volume_section_updater(struct Section *section) {
  FILE *fp = popen("pulsemixer --get-volume", "r");
  if (fp == NULL) {
    snprintf(section->text, SECTION_SIZE, "[Error]");
    return;
  }

  char buffer[SECTION_SIZE - 2];
  size_t n = fread(buffer, 1, SECTION_SIZE - 1, fp);
  buffer[n] = '\0';
  pclose(fp);

  char *newline = strchr(buffer, '\n');
  if (newline) {
    *newline = '\0';
  }

  snprintf(section->text, SECTION_SIZE, "[%s]", buffer);
}

void battery_section_updater(struct Section *section) {
  FILE *fp = popen("echo $(cat /sys/class/power_supply/BAT0/capacity)% $(cat "
                   "/sys/class/power_supply/BAT0/status)",
                   "r");
  if (fp == NULL) {
    snprintf(section->text, SECTION_SIZE, "[Battery?]");
    return;
  }

  char buffer[SECTION_SIZE - 2];
  size_t n = fread(buffer, 1, SECTION_SIZE - 1, fp);
  if (n > 0) {
    buffer[n - 1] = '\0';
  } else {
    buffer[0] = '\0';
  }
  pclose(fp);

  snprintf(section->text, SECTION_SIZE, "[%s]", buffer);
}

void setup_sections() {

  sections[0] = (Section){.timeout = 5, .updater = player_section_updater};

  sections[1] = (Section){.timeout = 1, .updater = time_section_updater};

  sections[2] = (Section){.timeout = 1, .updater = volume_section_updater};

  sections[3] = (Section){.timeout = 60, .updater = battery_section_updater};

  for (int i = 0; i < NSECTIONS; i++) {
    pids[i] = section_go(&sections[i]);
  }
}

void setup() {
  sections = mmap(NULL, sizeof(Section) * NSECTIONS, PROT_READ | PROT_WRITE,
                  MAP_SHARED | MAP_ANONYMOUS, -1, 0);
  if (sections == MAP_FAILED) {
    perror("mmap failed\n");
    exit(EXIT_FAILURE);
  }

  memset(bar_buffer, 0, sizeof(bar_buffer));
  display = XOpenDisplay(NULL);
  if (display == NULL) {
    perror("Failed to open display\n");
    exit(EXIT_FAILURE);
  }

  root = XDefaultRootWindow(display);

  setup_sections();
}

void cleanup() {
  if (display != NULL) {
    XCloseDisplay(display);
  }

  if (sections != NULL) {
    munmap(sections, sizeof(Section) * NSECTIONS);
  }
}

void handle_sigint(int sig) {
  for (int i = 0; i < NSECTIONS; i++) {
    if (pids[i] > 0) {
      kill(pids[i], SIGTERM);
    }
  }

  exit(EXIT_SUCCESS);
}

void run() {
  signal(SIGTERM, handle_sigint);
  signal(SIGKILL, handle_sigint);

  for (;;) {
    pause();
  }
}

int main() {
  atexit(cleanup);
  setup();
  run();
  return EXIT_SUCCESS;
}
