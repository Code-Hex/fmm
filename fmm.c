#include <stdio.h>
#include <stdlib.h>

#include <mach/mach.h>
#include <mach/host_info.h>

#include <string.h>
#include <errno.h>
#include <unistd.h>


#include <sys/sysctl.h> // total size
#include <sys/ioctl.h> // terminal width

#include "fmm.h"
#define LINE_LENGTH 80

int64_t getPageSize() {
    int64_t pagesize;
    size_t mem_len = sizeof(pagesize);
    int query[2];

    query[0] = CTL_HW;
    query[1] = HW_PAGESIZE; /* OSX */
    int result = sysctl(query, 2, &pagesize, &mem_len, NULL, 0);
    if(result == -1) {
        perror("sysctl");
        exit(-1);
    }
    return pagesize;
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

int digits(int num) {

    int digit = 0;
    while(num != 0){
        num = num / 10;
        ++digit;
    }

    return digit;
}

void MemoryStatus(double *a, double *w, double *i, double *u, double *t) {

    int cnt = 0;
    int width = getWidth();
    double loop_a = *a / *t * width;
    double loop_w = *w / *t * width;
    double loop_i = *i / *t * width;
    double loop_u = *u / *t * width;
    printf("%1.fMB\033[%1.fC%1.fMB\033[%1.fC%1.fMB\033[%1.fC%1.fMB\n",
        *a, loop_a - digits((int)*a) - 3, *w, loop_w - digits((int)*w) - 3, *i, loop_i - digits((int)*i) - 2, *u);

    printf("\e[43m");
    while (cnt++ < loop_a - 1) printf(" ");
    printf("\e[0m");

    cnt ^= cnt;
    printf("\e[41m");
    while (cnt++ < loop_w - 1) printf(" ");
    printf("\e[0m");


    cnt ^= cnt;
    printf("\e[44m");
    while (cnt++ < loop_i) printf(" ");
    printf("\e[0m");


    cnt ^= cnt;
    printf("\e[42m");
    while (cnt++ < loop_u) printf(" ");
    printf("\e[49m\n");

    printf("Active\033[%1.fCWired\033[%1.fCInactive\033[%1.fCFree\n",
        loop_a - 7, loop_w - 6, loop_i - 8);

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

    vm_size_t pagesize = getPageSize();
    mach_port_t mach_port = mach_host_self();
    mach_msg_type_number_t count = HOST_VM_INFO_COUNT;
    vm_statistics64_data_t vm_stat;
    double unit = 1024 << 10; /* 1024 * 1024 */
    
    if (KERN_SUCCESS == host_statistics64(mach_port, HOST_VM_INFO,
                                        (host_info64_t)&vm_stat, &count)) {
        

        double active = vm_stat.active_count * pagesize / unit; // Pages active
        //printf("active: %1.fMB\n", active);

        double inactive = vm_stat.inactive_count * pagesize / unit; // Pages inactive
        //printf("inactive: %1.fMB\n", inactive);

        double wired = vm_stat.wire_count * pagesize / unit; // Pages wired down
        //printf("wired: %1.fMB\n", wired); // passed

        
        double unused = vm_stat.free_count * pagesize / unit;
        //printf("PhysUnused: %1.fMB\n", unused);

        double compressor = vm_stat.compressor_page_count * pagesize / unit;
        //printf("Compressor: %1.fMB\n", compressor);

        double total = wired + active + inactive + compressor + unused;
        //printf("total: %1.fMB\n", total);

        MemoryStatus(&active, &wired, &inactive, &unused, &total);
        
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