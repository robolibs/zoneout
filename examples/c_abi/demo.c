#include "zoneout.h"

#include <stdio.h>

int main(void) {
  printf("zoneout version: %s\n", zoneout_version());
  const char *err = zoneout_last_error_message();
  printf("last error: %s\n", err ? err : "(none)");
  return 0;
}
