#!/usr/bin/env python

'''
--------------------------------------------
/proc/[number]/maps
http://linux.die.net/man/5/proc
--------------------------------------------
Info about [vsdo] and mem_layout for linux 32-bit machines

http://www.trilithium.com/johan/2005/08/linux-gate/
http://mail.nl.linux.org/kernelnewbies/2008-02/msg00251.html

    A file containing the currently mapped memory regions and their access 
    permissions.

    The format is:

    address       perms offset   dev   inode          pathname
00400000-00407000 r-xp 00000000 08:01 2998286        /bin/bzip2
00606000-00607000 r--p 00006000 08:01 2998286        /bin/bzip2
00607000-00608000 rw-p 00007000 08:01 2998286        /bin/bzip2
00608000-00609000 rw-p 00608000 00:00 0 
023a7000-023c8000 rw-p 023a7000 00:00 0              [heap]
7f2259348000-7f2259a68000 rw-p 7f2259348000 00:00 0 
7f2259a68000-7f2259bd0000 r-xp 00000000 08:01 91302  /lib/libc-2.9.so
7f2259bd0000-7f2259dd0000 ---p 00168000 08:01 91302  /lib/libc-2.9.so
7f2259dd0000-7f2259dd4000 r--p 00168000 08:01 91302  /lib/libc-2.9.so
7f2259dd4000-7f2259dd5000 rw-p 0016c000 08:01 91302  /lib/libc-2.9.so
7f2259dd5000-7f2259dda000 rw-p 7f2259dd5000 00:00 0 
7f2259dda000-7f2259de9000 r-xp 00000000 08:01 90150  /lib/libbz2.so.1.0.4
7f2259de9000-7f2259fe9000 ---p 0000f000 08:01 90150  /lib/libbz2.so.1.0.4
7f2259fe9000-7f2259fea000 r--p 0000f000 08:01 90150  /lib/libbz2.so.1.0.4
7f2259fea000-7f2259feb000 rw-p 00010000 08:01 90150  /lib/libbz2.so.1.0.4
7f2259feb000-7f225a00b000 r-xp 00000000 08:01 90161  /lib/ld-2.9.so
7f225a1ee000-7f225a1f0000 rw-p 7f225a1ee000 00:00 0 
7f225a206000-7f225a20a000 rw-p 7f225a206000 00:00 0 
7f225a20a000-7f225a20b000 r--p 0001f000 08:01 90161  /lib/ld-2.9.so
7f225a20b000-7f225a20c000 rw-p 00020000 08:01 90161  /lib/ld-2.9.so
7fff621f6000-7fff6220b000 rw-p 7ffffffea000 00:00 0  [stack]
7fff623fe000-7fff623ff000 r-xp 7fff623fe000 00:00 0  [vdso]
ffffffffff600000-ffffffffff601000 r-xp 00000000 00:00 0  [vsyscall]

    * address is the virtual address space in the process that it occupies
    * perms is a set of permissions:
       r = read
       w = write
       x = execute
       s = shared
       p = private (copy on write)

    * offset is the offset into the file/whatever
    * dev is the device (major:minor)
    * inode is the inode on that device. 0 indicates that no inode is 
      associated with the memory region, as the case would be with bss. 

******************************************************************************

http://www.mjmwired.net/kernel/Documentation/vm/pagemap.txt

http://selenic.com/repo/pagemap/

----------------------------------------
pagemap, from the userspace perspective
----------------------------------------

pagemap is a new (as of 2.6.25) set of interfaces in the kernel that allow
userspace programs to examine the page tables and related information by
reading files in /proc.

There are three components to pagemap:

 * /proc/pid/pagemap.  This file lets a userspace process find out which
   physical frame each virtual page is mapped to.  It contains one 64-bit
   value for each virtual page, containing the following data (from
   fs/proc/task_mmu.c, above pagemap_read):

    * Bits 0-54  page frame number (PFN) if present
    * Bits 0-4   swap type if swapped
    * Bits 5-54  swap offset if swapped
    * Bits 55-60 page shift (page size = 1<<page shift)
    * Bit  61    reserved for future use
    * Bit  62    page swapped
    * Bit  63    page present

   If the page is not present but in swap, then the PFN contains an
   encoding of the swap file number and the page's offset into the
   swap. Unmapped pages return a null PFN. This allows determining
   precisely which pages are mapped (or in swap) and comparing mapped
   pages between processes.

   Efficient users of this interface will use /proc/pid/maps to
   determine which areas of memory are actually mapped and llseek to
   skip over unmapped regions.

 * /proc/kpagecount.  This file contains a 64-bit count of the number of
   times each page is mapped, indexed by PFN.

 * /proc/kpageflags.  This file contains a 64-bit set of flags for each
   page, indexed by PFN.

   The flags are (from fs/proc/page.c, above kpageflags_read):

     0. LOCKED
     1. ERROR
     2. REFERENCED
     3. UPTODATE
     4. DIRTY
     5. LRU
     6. ACTIVE
     7. SLAB
     8. WRITEBACK
     9. RECLAIM
    10. BUDDY
    11. MMAP
    12. ANON
    13. SWAPCACHE
    14. SWAPBACKED
    15. COMPOUND_HEAD
    16. COMPOUND_TAIL
    16. HUGE
    18. UNEVICTABLE
    20. NOPAGE

Short descriptions to the page flags:

 0. LOCKED
    page is being locked for exclusive access, eg. by undergoing read/write IO

 7. SLAB
    page is managed by the SLAB/SLOB/SLUB/SLQB kernel memory allocator
    When compound page is used, SLUB/SLQB will only set this flag on the head
    page; SLOB will not flag it at all.

10. BUDDY
    a free memory block managed by the buddy system allocator
    The buddy system organizes free memory in blocks of various orders.
    An order N block has 2^N physically contiguous pages, with the BUDDY flag
    set for and _only_ for the first page.

15. COMPOUND_HEAD
16. COMPOUND_TAIL
    A compound page with order N consists of 2^N physically contiguous pages.
    A compound page with order 2 takes the form of "HTTT", where H donates its
    head page and T donates its tail page(s).  The major consumers of compound
    pages are hugeTLB pages (Documentation/vm/hugetlbpage.txt), the SLUB etc.
    memory allocators and various device drivers. However in this interface,
    only huge/giga pages are made visible to end users.
17. HUGE
    this is an integral part of a HugeTLB page

20. NOPAGE
    no page frame exists at the requested address

    [IO related page flags]
 1. ERROR     IO error occurred
 3. UPTODATE  page has up-to-date data
              ie. for file backed page: (in-memory data revision >= on-disk one)
 4. DIRTY     page has been written to, hence contains new data
              ie. for file backed page: (in-memory data revision >  on-disk one)
 8. WRITEBACK page is being synced to disk

    [LRU related page flags]
 5. LRU         page is in one of the LRU lists
 6. ACTIVE      page is in the active LRU list
18. UNEVICTABLE page is in the unevictable (non-)LRU list
                It is somehow pinned and not a candidate for LRU page reclaims,
		eg. ramfs pages, shmctl(SHM_LOCK) and mlock() memory segments
 2. REFERENCED  page has been referenced since last LRU list enqueue/requeue
 9. RECLAIM     page will be reclaimed soon after its pageout IO completed
11. MMAP        a memory mapped page
12. ANON        a memory mapped page that is not part of a file
13. SWAPCACHE   page is mapped to swap space, ie. has an associated swap entry
14. SWAPBACKED  page is backed by swap/RAM

The page-types tool in this directory can be used to query the above flags.

Using pagemap to do something useful:

The general procedure for using pagemap to find out about a process' memory
usage goes like this:

 1. Read /proc/pid/maps to determine which parts of the memory space are
    mapped to what.
 2. Select the maps you are interested in -- all of them, or a particular
    library, or the stack or the heap, etc.
 3. Open /proc/pid/pagemap and seek to the pages you would like to examine.
 4. Read a u64 for each page from pagemap.
 5. Open /proc/kpagecount and/or /proc/kpageflags.  For each PFN you just
    read, seek to that entry in the file, and read the data you want.

For example, to find the "unique set size" (USS), which is the amount of
memory that a process is using that is not shared with any other process,
you can go through every map in the process, find the PFNs, look those up
in kpagecount, and tally up the number of pages that are only referenced
once.

Other notes:
-----------------
Reading from any of the files will return -EINVAL if you are not starting
the read on an 8-byte boundary (e.g., if you seeked an odd number of bytes
into the file), or if the size of the read is not a multiple of 8 bytes.

'''

import os, array, sys, struct, re

#these are #defines equivalents :P
PAGE_SIZE = 4096
BYTE = 8

pid = int(sys.argv[1])
#pid = os.getpid()

#addr = int(sys.argv[2])

class processmap(object):
    def __init__(self, pid):
        self._pid = pid
        self._maps = []
        self._mapcache = {}
        self.readmap()

    def readmap(self):
        self._maps = []
        self._mapcache = {}
        self._empty = "\xff" * 1024
        class mapping(object): pass
        for l in file("/proc/%s/maps" % self._pid):
            m = re.match(r"(\w+)-(\w+) (\S+) (\S+) (\S+) (\d+)\s+(\S*)", l)
            start, end, prot, offset, dev, huh, name = m.groups()
            a = mapping()
            a.start = int(start, 16)
            a.end = int(end, 16)
            a.offset = int(offset, 16)
            a.prot = prot
            a.dev = dev
            a.name = name
            #print "%08x-%08x %08x %s %s" % (a.start,a.end,a.offset,a.prot,
            #a.name)
            self._maps.append(a)
        # 6*2^20 Bytes are read coz there are 2^20 page of 4KB each in a 
        # 32-bit machine. Why 6? Dunno yet :-)
        self.data = file("/proc/%s/pagemap" % self._pid, "r", 0).read(6*2**20)

    def maps(self):
        return self._maps

    # given an address range, this returns an array with 
    # PFN for each VA page
    def range_to_pfn(self, startaddr, endaddr):
        off = (startaddr / 4096) * 8
        size = ((endaddr - startaddr) / 4096)

        for i in range(size) : 
            d = self.data[off+i*8:off+(i+1)*8]
            if len(d):
                q = "Q"
                l = struct.unpack(q,d)
                arr_here = array.array("L", l)
                pg_present = arr_here[0] >> 63
                pg_swapped = ((arr_here[0] >> 62) & 1)
                # 1F80000000000000 = 2269814212194729984
                pg_shift = ((arr_here[0] & 2269814212194729984) >> 55)
                pg_size = 1 << pg_shift
                # 0x7FFFFFFFFFFFFF = 36028797018963967
                pfn = (arr_here[0] & 36028797018963967)

                if pg_present and not pg_swapped and not pg_size == 4096 : 
                    raise AssertionError
                
                if pg_present and not pg_swapped :
                    vpn = off+i*8
                    addr = startaddr + i*4096
                    print "addr = %08x VPN = %08x, PFN = %08x" % (addr,vpn,pfn)

        return []
        fm = file("/proc/%s/pagemap" % self._pid, "r", 0) # uncached
        fm.seek(off)
        return array.array("L", fm.read(size))


# Begin main section

def main():

    #print "got pid = %d" % os.getpid()

    map_for_pid = processmap(pid)

    # mr is a list of tuples of start address, end addr, and name
    mr = [(m.start, m.end, m.name) for m in map_for_pid.maps()]

    for i in range(len(mr)):
        print "%08x-%08x %s" % (mr[i][0],mr[i][1],mr[i][2])

    print " "

    for k in range(len(mr)):
        arr = map_for_pid.range_to_pfn(mr[k][0],mr[k][1])

    print " "

if __name__ == "__main__":
    main()

