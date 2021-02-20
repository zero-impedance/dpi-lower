#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <sys/time.h>
#include <unistd.h>

#define DBL_CLICK_INTERVAL 0.5

FILE *make_uinput(void) {
  FILE *fout = fopen("/dev/uinput", "wb+");
  int fd_out = fileno(fout);

  ioctl(fd_out, UI_SET_EVBIT, EV_REL);
  ioctl(fd_out, UI_SET_RELBIT, REL_X);
  ioctl(fd_out, UI_SET_RELBIT, REL_Y);
  ioctl(fd_out, UI_SET_RELBIT, REL_Z);
  ioctl(fd_out, UI_SET_RELBIT, REL_DIAL);
  ioctl(fd_out, UI_SET_RELBIT, REL_WHEEL);
  ioctl(fd_out, UI_SET_RELBIT, REL_MISC);
  ioctl(fd_out, UI_SET_EVBIT, EV_KEY);
  ioctl(fd_out, UI_SET_KEYBIT, BTN_LEFT);
  ioctl(fd_out, UI_SET_KEYBIT, BTN_RIGHT);
  ioctl(fd_out, UI_SET_KEYBIT, BTN_MIDDLE);
  ioctl(fd_out, UI_SET_EVBIT, EV_SYN);

  struct uinput_user_dev uidev = {0};
  snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "DPI-LOWERED-MOUSE");
  uidev.id.bustype = BUS_USB;
  uidev.id.vendor = 0x7777;
  uidev.id.product = 0x7777;
  uidev.id.version = 1;

  fwrite(&uidev, sizeof(uidev), 1, fout);
  fflush(fout);

  if (ioctl(fd_out, UI_DEV_CREATE) < 0) {
    printf("ERROR: UI_DEV_CREATE\n");
    exit(1);
  }

  return fout;
}

void send_event(uint16_t type, uint16_t code, int16_t value, FILE *fout) {
    struct input_event ev;
    gettimeofday(&ev.time, NULL);
    ev.type = type;
    ev.code = code;
    ev.value = value;
    fwrite(&ev, sizeof(ev), 1, fout);
    fflush(fout);
    //printf("[%lu.%03d] type=%u code=%u value=%d\n",
    //       ev.time.tv_sec, (int) (ev.time.tv_usec / 1000), ev.type, ev.code, ev.value);
}

double get_time(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec + tv.tv_usec / 1000000.0;
}

void loop(const char *dev, double fast_rate, double slow_rate) {
  FILE *fin = fopen(dev, "rb");
  int fd_in = fileno(fin);

  // don't pass event to default handler
  ioctl(fd_in, EVIOCGRAB, 1);

  FILE *fout = make_uinput();

  struct input_event ev;
  double rate = fast_rate;
  double x_acc = 0;
  double y_acc = 0;
  double t_last = 0;
  int rpressed = 0;
  while (1) {
    fread(&ev, sizeof(ev), 1, fin);
    if (ev.type == EV_KEY && ev.code == BTN_RIGHT) {
      if (ev.value == 1) {
        double t = get_time();
        if (t - t_last <= DBL_CLICK_INTERVAL) {
          send_event(EV_KEY, BTN_RIGHT, 1, fout);
          send_event(EV_SYN, SYN_REPORT, 0, fout);
          rpressed = 1;
          t_last = 0;
        } else {
          rate = slow_rate;
          t_last = t;
        }
      } else {
        if (rpressed) {
          send_event(EV_KEY, BTN_RIGHT, 0, fout);
          send_event(EV_SYN, SYN_REPORT, 0, fout);
          rpressed = 0;
        }
        rate = fast_rate;
      }
    } else if (ev.type == EV_REL) {
      if (ev.code == REL_X) {
        x_acc += ev.value * rate;
        if (x_acc <= -1 || 1 <= x_acc) {
          send_event(EV_REL, REL_X, (int) x_acc, fout);
          x_acc -= (int) x_acc;
        }
      } else if (ev.code == REL_Y) {
        y_acc += ev.value * rate;
        if (y_acc <= -1 || 1 <= y_acc) {
          send_event(EV_REL, REL_Y, (int) y_acc, fout);
          y_acc -= (int) y_acc;
        }
      } else {
        send_event(ev.type, ev.code, ev.value, fout);
      }
    } else if (ev.type == EV_SYN || ev.type == EV_KEY) {
      send_event(ev.type, ev.code, ev.value, fout);
    }
  }
}

int main(int argc, char *argv[]) {
  if (argc < 4) {
    fprintf(stderr, "Usage: dpi-lower.bin dev fast-rate slow-rate\n");
    return 1;
  }

  const char *dev = argv[1];
  double fast_rate = atof(argv[2]);
  double slow_rate = atof(argv[3]);

  loop(dev, fast_rate, slow_rate);

  return 0;
}
