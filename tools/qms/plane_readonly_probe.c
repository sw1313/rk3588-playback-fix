/* SPDX-License-Identifier: MIT
 * Bounded structural scanout observation, without reading any pixel buffers.
 * This samples the factory debugfs summary, not wire pixels or a hardware CRC.
 * Missing between-sample changes and black pixels inside an enabled plane
 * remain unknown. No DRM modeset, input, QMS command, or register writes.
 */
#define _POSIX_C_SOURCE 200809L
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t stopping;
static void stop(int sig) { (void)sig; stopping=1; }
static uint64_t now(clockid_t id)
{
    struct timespec t;
    if (clock_gettime(id,&t)) return 0;
    return (uint64_t)t.tv_sec*1000000000ULL+(uint64_t)t.tv_nsec;
}
static void quoted(const char *p)
{
    putchar('"');
    for (; *p; ++p) {
        unsigned char c=(unsigned char)*p;
        if (c=='"' || c=='\\') { putchar('\\'); putchar(c); }
        else if (c=='\n') fputs("\\n",stdout);
        else if (c=='\r') fputs("\\r",stdout);
        else if (c=='\t') fputs("\\t",stdout);
        else if (c<32) printf("\\u%04x",c);
        else putchar(c);
    }
    putchar('"');
}
static int keep(const char *s)
{
    /* Buffer addresses change on every frame and reveal private scanout
     * locations. Retain only state/format/alpha/placement, never buf[] lines.
     * Software Display mode remains the native base mode with QMS: it is
     * deliberately excluded rather than presented as actual output Hz.
     */
    static const char *keys[]={"Video Port", "Connector:", "bus_format[",
        "overlay_mode[", "win_id:", "format:", "rotate:", "csc:",
        "zpos:", "src:", "dst:"};
    if (strstr(s,"-win") && (strstr(s,": ACTIVE") || strstr(s,": DISABLED"))) return 1;
    for (unsigned i=0; i<sizeof(keys)/sizeof(*keys); ++i)
        if (!strncmp(s,keys[i],strlen(keys[i]))) return 1;
    return 0;
}
static int summary(char *signature,size_t capacity)
{
    char raw[16384]; size_t n=0,used=0;
    int fd=open("/sys/kernel/debug/dri/0/summary",O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
    if (fd<0) return -1;
    while (n<sizeof(raw)-1) {
        ssize_t got=read(fd,raw+n,sizeof(raw)-1-n);
        if (got<0 && errno==EINTR && !stopping) continue;
        if (got<0) { int saved=errno; close(fd); errno=saved; return -1; }
        if (!got) break;
        n+=(size_t)got;
    }
    close(fd);
    if (!n || n==sizeof(raw)-1) { errno=EOVERFLOW; return -1; }
    raw[n]=0; signature[0]=0;
    unsigned ports=0; char *context=NULL;
    for (char *line=strtok_r(raw,"\n",&context); line; line=strtok_r(NULL,"\n",&context)) {
        while (*line==' ' || *line=='\t') ++line;
        if (!strncmp(line,"Video Port",10)) ++ports;
        if (!keep(line)) continue;
        size_t size=strlen(line);
        if (used+size+2>capacity) { errno=EOVERFLOW; return -1; }
        memcpy(signature+used,line,size); used+=size; signature[used++]='\n'; signature[used]=0;
    }
    if (ports!=4) { errno=EPROTO; return -1; }
    return 0;
}
int main(int argc,char **argv)
{
    char *end; unsigned long seconds;
    if (getuid()!=0 || argc!=2 || !*argv[1]) return 2;
    errno=0; seconds=strtoul(argv[1],&end,10);
    if (errno || *end || seconds<1 || seconds>180) return 2;
    signal(SIGINT,stop); signal(SIGTERM,stop); setvbuf(stdout,NULL,_IOLBF,0);
    char previous[8192]="",current[8192];
    uint64_t begin=now(CLOCK_MONOTONIC),deadline=begin+seconds*1000000000ULL;
    uint64_t target=begin,last=0,max_gap=0,total_read=0,max_read=0;
    unsigned samples=0,changes=0,overruns=0;
    puts("{\"diagnostic_only\":true,\"interval_ms\":50,\"pixel_buffer_reads\":false,\"hardware_CRC\":false,\"output_refresh_rate_measured\":false}");
    while (!stopping && now(CLOCK_MONOTONIC)<deadline) {
        uint64_t start=now(CLOCK_MONOTONIC);
        if (summary(current,sizeof(current))) { perror("structural summary unavailable; stopped"); return 3; }
        uint64_t finish=now(CLOCK_MONOTONIC),duration=finish-start;
        if (last && start-last>max_gap) max_gap=start-last;
        last=start; total_read+=duration; if (duration>max_read) max_read=duration; ++samples;
        if (strcmp(previous,current)) {
            printf("{\"mono_start_ns\":%llu,\"mono_end_ns\":%llu,\"wall_ns\":%llu,\"sample\":%u,\"structure\":",
                (unsigned long long)start,(unsigned long long)finish,
                (unsigned long long)now(CLOCK_REALTIME),samples);
            quoted(current); puts("}"); strcpy(previous,current); ++changes;
        }
        target+=50000000ULL;
        if (finish>=target) { ++overruns; target=finish+50000000ULL; }
        struct timespec wake={(time_t)(target/1000000000ULL),(long)(target%1000000000ULL)};
        int ret;
        do { ret=clock_nanosleep(CLOCK_MONOTONIC,TIMER_ABSTIME,&wake,NULL); }
        while (ret==EINTR && !stopping);
        if (ret && ret!=EINTR) { errno=ret; perror("clock_nanosleep"); return 4; }
    }
    printf("{\"samples_completed\":%u,\"structural_snapshots\":%u,\"read_total_ns\":%llu,\"read_max_ns\":%llu,\"sample_gap_max_ns\":%llu,\"overruns\":%u,\"elapsed_ns\":%llu,\"interrupted\":%s}\n",
        samples,changes,(unsigned long long)total_read,(unsigned long long)max_read,
        (unsigned long long)max_gap,overruns,(unsigned long long)(now(CLOCK_MONOTONIC)-begin),
        stopping?"true":"false");
    return 0;
}
