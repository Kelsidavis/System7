/*
 * sched.h - Minimal scheduler definitions for bare-metal ARM64
 */

#ifndef _SCHED_H_
#define _SCHED_H_

#ifdef __cplusplus
extern "C" {
#endif

/* Minimal scheduling definitions - most are no-ops in bare-metal */

/* Process ID type */
typedef int pid_t;

/* Scheduling policies */
#define SCHED_OTHER  0
#define SCHED_FIFO   1
#define SCHED_RR     2

/* CPU affinity */
typedef unsigned long cpu_set_t;

#define CPU_ZERO(set)              (*(set) = 0)
#define CPU_SET(cpu, set)          (*(set) |= (1UL << (cpu)))
#define CPU_CLR(cpu, set)          (*(set) &= ~(1UL << (cpu)))
#define CPU_ISSET(cpu, set)        ((*(set) & (1UL << (cpu))) != 0)
#define CPU_COUNT(set)             (__builtin_popcountl(*(set)))

/* Scheduling parameter structure */
struct sched_param {
    int sched_priority;
};

/* Minimal scheduler functions - stubs for bare-metal */
int sched_yield(void);

#ifdef __cplusplus
}
#endif

#endif /* _SCHED_H_ */
