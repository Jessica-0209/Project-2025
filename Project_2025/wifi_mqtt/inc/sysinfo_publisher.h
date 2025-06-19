#ifndef SYSINFO_PUBLISHER_H
#define SYSINFO_PUBLISHER_H

void *run_sysinfo_publisher(void *arg);

const char *get_hostname();
double get_cpu_usage();
double get_memory_usage();

#endif 
