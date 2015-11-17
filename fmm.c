#include <stdio.h>
#include <stdlib.h>

#include <mach/mach.h>

#include <errno.h>
#include <unistd.h> // page size
#include <sys/sysctl.h> // total size
#include <sys/ioctl.h> // terminal width
#include "fmm.h"

#define LINE_LENGTH 80

typedef struct vm {
    double pagesize;
    double active; // Pages active
    double inactive; // Pages inactive
    double wired; // Pages wired down
    double speculative; // Pages speculative
    double free; // Pages free
    double compressor; // Pages occupied by compressor
    double unused; // free - speculative
    struct vm *next;
} data64_vm;

data64_vm vmdata;

static 
vm_statistics64_data_t getVMinfo() {
    vm_statistics64_data_t vm_stat;
    mach_port_t mach_port = mach_host_self();
    mach_msg_type_number_t count = HOST_VM_INFO_COUNT;

    if (KERN_SUCCESS != host_statistics64(mach_port, HOST_VM_INFO,
                                        (host_info64_t)&vm_stat, &count)) {
        perror("host_statistics64");
        exit(-1);
    }
    return vm_stat;
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

void MemoryStatus(double a, double w, double i, double u, double t) {

    int width = getWidth();
    double loop_a = a / t * width;
    double loop_w = w / t * width;
    double loop_i = i / t * width;
    double loop_u = u / t * width;
    printf("%1.f\n", loop_u + loop_i + loop_w + loop_a);
    printf("\e[43m\e[97mActive  : %*.fMB\e[49m "
           "\e[41m\e[97mWired   : %*.fMB\e[49m "
           "\e[44m\e[97minactive: %*.fMB\e[49m "
           "\e[42m\e[97mFree    : %*.fMB\e[49m\n", 4, a, 4, w, 4, i, 4, u);

    printf("\e[43m%*s", (int)loop_a, " ");
    printf("\e[41m%*s", (int)loop_w, " ");
    printf("\e[44m%*s", (int)loop_i, " ");
    printf("\e[42m%*s\e[49m\n", (int)loop_u, " ");

    //printf("Active\033[%1.fCWired\033[%1.fCInactive\033[%dCFree\n",loop_a - 7, loop_w - 6, width - 34);

    /*
        wired is red \e[41m
        free is green \e[42m
        active is yellow \e[43m
        inactive is blue \e[44m
        reset color \e[0m
        return color \e[49m
        [yellow red blue green]
    */
}

size_t getTotalSystemMemory()
{
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGE_SIZE);
    return pages * page_size;
}

int main(int argc, char const *argv[]) {
    vm_size_t pagesize = sysconf(_SC_PAGE_SIZE);
    vm_statistics64_data_t vm_stat;
    uint64_t unit = 1024 << 10; /* 1024 * 1024 */
    
    vmdata.next = NULL;
    vm_stat = getVMinfo();

    vmdata.active = vm_stat.active_count * pagesize / unit; // Pages active
    vmdata.inactive = vm_stat.inactive_count * pagesize / unit; // Pages inactive
    vmdata.wired = vm_stat.wire_count * pagesize / unit; // Pages wired down
    vmdata.speculative = vm_stat.speculative_count * pagesize / unit; // Pages speculative
    vmdata.free = vm_stat.free_count * pagesize / unit; // Pages free
    vmdata.compressor = vm_stat.compressor_page_count * pagesize / unit; // Pages occupied by compressor
    vmdata.unused = vmdata.free - vmdata.speculative;

    double total = vmdata.active + vmdata.active + vmdata.inactive + vmdata.compressor + vmdata.unused;
    printf("total: %1.fMB\n", total);

    MemoryStatus(vmdata.active, vmdata.wired, vmdata.inactive, vmdata.unused, total);

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