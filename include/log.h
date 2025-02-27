#ifndef LOG_H
#define LOG_H

int verbose;

void verbose_log(const char *format, ...);
void error_log(const char *format, ...);

#endif
