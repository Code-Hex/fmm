#include <stdio.h>
#include <stdlib.h>

#include <mach/mach.h>

#include <errno.h>
#include <unistd.h> // page size
#include <sys/syscall.h> // purge
#include <sys/sysctl.h> // total size
#include <sys/ioctl.h> // terminal width
#include "fmm.h"

#define LINE_LENGTH 80

typedef struct vm {
    vm_size_t pagesize;
    double active; // Pages active
    double inactive; // Pages inactive
    double wired; // Pages wired down
    double speculative; // Pages speculative
    double free; // Pages free
    double compressor; // Pages occupied by compressor
    double unused; // free - speculative
    double total;
} data64_vm;

data64_vm vmdata;

static 
void getVMinfo(vm_statistics64_t stat) {
    mach_port_t mach_port = mach_host_self();
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;

    if (KERN_SUCCESS != host_statistics64(mach_port, HOST_VM_INFO,
                                        (host_info64_t)stat, &count)) {
        perror("host_statistics64");
        exit(-1);
    }
}

int getWidth() {
    struct winsize ws;
    int result = ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws);
    if(result == -1) {
        perror("ioctl");
        exit(-1);
    }
    return (0 < ws.ws_col && ws.ws_col == (int)ws.ws_col) ? ws.ws_col : LINE_LENGTH;
}

void MemoryStatus() {

    int width = getWidth();
    double a = vmdata.active;
    double w = vmdata.wired;
    double i = vmdata.inactive;
    double c = vmdata.compressor;
    double u = vmdata.unused;
    double t = vmdata.total;

    double loop_a = a / t * width;
    double loop_w = w / t * width;
    double loop_i = i / t * width;
    double loop_c = c / t * width;
    double loop_u = u / t * width;
    printf("\e[43m  \e[49m Active  : %*.fMB\n"
           "\e[41m  \e[49m Wired   : %*.fMB\n"
           "\e[44m  \e[49m inactive: %*.fMB\n"
           "\e[45m  \e[49m compress: %*.fMB\n"
           "\e[42m  \e[49m Free    : %*.fMB\n\e[49m\n", 4, a, 4, w, 4, i, 4, c, 4, u);
    printf("\e[43m%*s", (int)loop_a, " ");
    printf("\e[41m%*s", (int)loop_w, " ");
    printf("\e[44m%*s", (int)loop_i, " ");
    printf("\e[45m%*s", (int)loop_c, " ");
    int total = (int)loop_u + (int)loop_c + (int)loop_w + (int)loop_i + (int)loop_a;
    while (width > ++total) printf(" ");
    printf("\e[42m%*s\e[49m\n", (int)loop_u, " ");

    /*
        wired is red \e[41m
        free is green \e[42m
        active is yellow \e[43m
        inactive is blue \e[44m
        compress is \e[45m
        reset color \e[0m
        return color \e[49m
        [yellow red blue  green]
    */
}

void purge() {
    printf("purged:\n");
    int result = syscall(SYS_vfs_purge);
    if (result) {
        perror("Unable to purge disk buffers");
        exit(-1);
    }
    sleep(1);
}

size_t getTotalSystemMemory() {
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);
    return pages * page_size;
}


void getStatus() {
    vmdata.pagesize = sysconf(_SC_PAGE_SIZE);
    vm_statistics64_data_t vm_stat;
    uint64_t unit = 1024 << 10; /* 1024 * 1024 */
    
    getVMinfo(&vm_stat);

    vmdata.active = vm_stat.active_count * vmdata.pagesize / unit; // Pages active
    vmdata.inactive = vm_stat.inactive_count * vmdata.pagesize / unit; // Pages inactive
    vmdata.wired = vm_stat.wire_count * vmdata.pagesize / unit; // Pages wired down
    vmdata.speculative = vm_stat.speculative_count * vmdata.pagesize / unit; // Pages speculative
    vmdata.free = vm_stat.free_count * vmdata.pagesize / unit; // Pages free
    vmdata.compressor = vm_stat.compressor_page_count * vmdata.pagesize / unit; // Pages occupied by compressor
    vmdata.unused = vmdata.free - vmdata.speculative;
    vmdata.total = vmdata.active + vmdata.wired + vmdata.inactive + vmdata.compressor + vmdata.unused;
}

void usage() {
    fprintf(stderr, "Usage: fmm [options]\n"
            "  fmm is friendly memory monitoring tool.\n"
            "  Options include:\n"
            "  -l <loop>             - show status for per second\n"
            "  -p <purge>            - use purge for you must become root user\n"
            "  \n"
           );
    exit(0);
}

void loopStatus() {
    while (1) {
        system("clear");
        printf("Exit on ^C\n\n");
        getStatus();
        MemoryStatus();
        sleep(1);
    }
}

int main(int argc, char * const argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "hkpl")) != 1) {
        switch (opt) {
            case 'h':
                usage();
            case 'k':
                exit(0); // show like top and kill pid
            case 'p':
                getStatus();
                MemoryStatus();
                purge();
                getStatus();
                MemoryStatus();
                exit(0);
            case 'l':
                loopStatus();
                break;
            default:
                getStatus();
                MemoryStatus();
                exit(0);
        }

    }
    return 0;
}

/*
    How to calculate total physical memory 
    http://opensource.apple.com/source/top/top-100.1.2/globalstats.c

    Look up this
    http://www.opensource.apple.com/source/xnu/xnu-1456.1.26/osfmk/mach/vm_statistics.h

    double free = (vm_stat.free_count - vm_stat.speculative_count) * pagesize / unit; // Pages free
    double decompressions = vm_stat.decompressions; // Decompressions
    double compressions = vm_stat.compressions; // Compressions
    double pageins = vm_stat.pageins; // Pageins
    double pageouts = vm_stat.pageouts; // Pageouts
    double swapins = vm_stat.swapins; // Swapins
    double swapouts = vm_stat.swapouts; // Swapouts
    double cow = vm_stat.cow_faults; // Pages copy-on-write
    double reactivations = vm_stat.reactivations; // Pages reactivated
    double purges = vm_stat.purges; // Pages purged
    double total_uncompressed = vm_stat.total_uncompressed_pages_in_compressor; // Pages stored in compressor
    double throttled = vm_stat.throttled_count; // Pages throttled
    double purgeable = vm_stat.purgeable_count; // Pages purgeable
    double zero = vm_stat.zero_fill_count; // Pages zero filled
    double external = vm_stat.external_page_count; // File-backed pages
    double internal = vm_stat.internal_page_count; // Anonymous pages
*/