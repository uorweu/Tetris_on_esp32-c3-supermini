/* main.c - ESP32-C3 entry point. Bring up the platform, then hand off to the
 * shared app loop. Everything architecture-interesting lives in common/. */
#include "platform.h"
#include "app.h"

void app_main(void)
{
    platform_init();
    app_run();          /* never returns on embedded */
}
