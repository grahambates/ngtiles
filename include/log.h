#ifndef LOG_H
#define LOG_H

extern int verbose;

void verbose_log(const char *format, ...);
void error_log(const char *format, ...);

#endif // LOG_H
