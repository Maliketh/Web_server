#ifndef __REQUEST_H__

void requestHandle(int fd, int thread_id, int *static_stats, int *dynamic_stats, int *total_stats, struct timeval arrival_time, struct timeval dispatch_time);

#endif
