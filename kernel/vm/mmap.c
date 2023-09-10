/******************************************************************************/
/* Important Spring 2022 CSCI 402 usage information:                          */
/*                                                                            */
/* This fils is part of CSCI 402 kernel programming assignments at USC.       */
/*         53616c7465645f5fd1e93dbf35cbffa3aef28f8c01d8cf2ffc51ef62b26a       */
/*         f9bda5a68e5ed8c972b17bab0f42e24b19daa7bd408305b1f7bd6c7208c1       */
/*         0e36230e913039b3046dd5fd0ba706a624d33dbaa4d6aab02c82fe09f561       */
/*         01b0fd977b0051f0b0ce0c69f7db857b1b5e007be2db6d42894bf93de848       */
/*         806d9152bd5715e9                                                   */
/* Please understand that you are NOT permitted to distribute or publically   */
/*         display a copy of this file (or ANY PART of it) for any reason.    */
/* If anyone (including your prospective employer) asks you to post the code, */
/*         you must inform them that you do NOT have permissions to do so.    */
/* You are also NOT permitted to remove or alter this comment block.          */
/* If this comment block is removed or altered in a submitted file, 20 points */
/*         will be deducted.                                                  */
/******************************************************************************/

#include "globals.h"
#include "errno.h"
#include "types.h"

#include "mm/mm.h"
#include "mm/tlb.h"
#include "mm/mman.h"
#include "mm/page.h"

#include "proc/proc.h"

#include "util/string.h"
#include "util/debug.h"

#include "fs/vnode.h"
#include "fs/vfs.h"
#include "fs/file.h"

#include "vm/vmmap.h"
#include "vm/mmap.h"

/*
 * This function implements the mmap(2) syscall, but only
 * supports the MAP_SHARED, MAP_PRIVATE, MAP_FIXED, and
 * MAP_ANON flags.
 *
 * Add a mapping to the current process's address space.
 * You need to do some error checking; see the ERRORS section
 * of the manpage for the problems you should anticipate.
 * After error checking most of the work of this function is
 * done by vmmap_map(), but remember to clear the TLB.
 */
int
do_mmap(void *addr, size_t len, int prot, int flags,
        int fd, off_t off, void **ret)
{
        
        if(!(flags&(MAP_SHARED|MAP_PRIVATE))){
                dbg(DBG_PRINT, "(GRADING3D)\n");
                return -EINVAL;
        }

        // INVALID len
        if(!PAGE_ALIGNED(off)||len<=0||len>(USER_MEM_HIGH-USER_MEM_LOW)){
                dbg(DBG_PRINT, "(GRADING3D)\n");
                return -EINVAL;
        }

       

        // not aligned
        if(!PAGE_ALIGNED(addr)){
                dbg(DBG_PRINT, "(GRADING3D)\n");
                return -EINVAL;
        }

        uint32_t lopage = 0;

        if(flags&MAP_FIXED){
                 // invalid addr
                if(addr==NULL||(uint32_t)addr<USER_MEM_LOW||(uint32_t)addr+len>USER_MEM_HIGH){
                        dbg(DBG_PRINT, "(GRADING3D)\n");
                        return -EINVAL;
                }
                lopage = ADDR_TO_PN(addr);
                dbg(DBG_PRINT, "(GRADING3D)\n");
        }

		
        vnode_t* node = NULL;
        file_t* file;
        if(!(flags&MAP_ANON)){
                // dbg(DBG_PRINT, "(GRADING3B)\n");
                // node = curproc->p_files[fd]->f_vnode;
                if(fd < 0 || fd >= NFILES)
                {
                        dbg(DBG_PRINT, "(GRADING3D)\n");
                        return -EBADF;       
                }
                file = fget(fd);
                if(file==NULL){
                        dbg(DBG_PRINT, "(GRADING3D)\n");
                        return -EBADF;
                }

                if ((flags & MAP_SHARED) && (prot & PROT_WRITE) && (file->f_mode == FMODE_READ))
                {       
                        dbg(DBG_PRINT, "(GRADING3D)\n");
                        fput(file);
                        
                        return -EINVAL;
                }
                dbg(DBG_PRINT, "(GRADING3D)\n");
                node = file->f_vnode; 
        }
	
        vmarea_t* area;

        // do vmmap_map()
        int res = vmmap_map(curproc->p_vmmap,node,lopage,(len-1)/PAGE_SIZE+1,prot,flags,off,VMMAP_DIR_HILO,&area);

        if (res < 0)
        {
                dbg(DBG_PRINT, "(GRADING3D)\n");
                if(!(flags&MAP_ANON)){
                        fput(file);
                        dbg(DBG_PRINT, "(GRADING3D)\n");
                }
                dbg(DBG_PRINT, "(GRADING3D)\n");
                return res;
        }

        *ret = PN_TO_ADDR(area->vma_start);

        // clear the TLB

        pt_unmap_range(curproc->p_pagedir, (uintptr_t) PN_TO_ADDR(area->vma_start),
               (uintptr_t) PN_TO_ADDR(area->vma_start)
               + (uintptr_t) PAGE_ALIGN_UP(len));


        
        tlb_flush_range((uintptr_t) PN_TO_ADDR(area->vma_start),(uint32_t)((len-1)/PAGE_SIZE+1));


        
        if(!(flags&MAP_ANON)){
                fput(file);
                dbg(DBG_PRINT, "(GRADING3D)\n");
        }
        KASSERT(NULL != curproc->p_pagedir);
        dbg(DBG_PRINT, "(GRADING3A 2.a)\n");
        dbg(DBG_PRINT, "(GRADING3D)\n");
        return res;
}


/*
 * This function implements the munmap(2) syscall.
 *
 * As with do_mmap() it should perform the required error checking,
 * before calling upon vmmap_remove() to do most of the work.
 * Remember to clear the TLB.
 */
int
do_munmap(void *addr, size_t len)
{
        
       // INVALID len
        if((uint32_t)len<=0||(uint32_t)len>=USER_MEM_HIGH){
                
                dbg(DBG_PRINT, "(GRADING3D)\n");
                return -EINVAL;
        }

        // invalid addr
        if((uint32_t)addr<=0||(uint32_t)addr<=USER_MEM_LOW||(uint32_t)addr>=USER_MEM_HIGH){
                dbg(DBG_PRINT, "(GRADING3D)\n");
                return -EINVAL;
        }

        // not aligned
        if(!PAGE_ALIGNED(addr)){
                dbg(DBG_PRINT, "(GRADING3D)\n");
                return -EINVAL;
        }

        int res = vmmap_remove(curproc->p_vmmap,ADDR_TO_PN(addr),(uint32_t)((len-1)/PAGE_SIZE+1));
        dbg(DBG_PRINT, "(GRADING3D)\n");
        tlb_flush_all();
        return res;
}

