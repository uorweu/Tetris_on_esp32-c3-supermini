/* app.h - the high-level loop, shared by both targets.
 * Each target's main() just calls app_run() after platform_init(). */
#ifndef APP_H
#define APP_H

void app_run(void);   /* never returns on embedded; returns on host quit */

#endif
