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

#include "types.h"
#include "globals.h"
#include "errno.h"

#include "util/debug.h"
#include "util/string.h"

#include "proc/proc.h"
#include "proc/kthread.h"

#include "mm/mm.h"
#include "mm/mman.h"
#include "mm/page.h"
#include "mm/pframe.h"
#include "mm/mmobj.h"
#include "mm/pagetable.h"
#include "mm/tlb.h"

#include "fs/file.h"
#include "fs/vnode.h"

#include "vm/shadow.h"
#include "vm/vmmap.h"

#include "api/exec.h"

#include "main/interrupt.h"

/* Pushes the appropriate things onto the kernel stack of a newly forked thread
 * so that it can begin execution in userland_entry.
 * regs: registers the new thread should have on execution
 * kstack: location of the new thread's kernel stack
 * Returns the new stack pointer on success. */
static uint32_t
fork_setup_stack(const regs_t *regs, void *kstack)
{
        /* Pointer argument and dummy return address, and userland dummy return
         * address */
        uint32_t esp = ((uint32_t) kstack) + DEFAULT_STACK_SIZE - (sizeof(regs_t) + 12);
        *(void **)(esp + 4) = (void *)(esp + 8); /* Set the argument to point to location of struct on stack */
        memcpy((void *)(esp + 8), regs, sizeof(regs_t)); /* Copy over struct */
        return esp;
}


/*
 * The implementation of fork(2). Once this works,
 * you're practically home free. This is what the
 * entirety of Weenix has been leading up to.
 * Go forth and conquer.
 */
int
do_fork(struct regs *regs)
{
        KASSERT(regs != NULL);
        dbg(DBG_PRINT, "(GRADING3A 7.a)\n");
        KASSERT(curproc != NULL);
        dbg(DBG_PRINT, "(GRADING3A 7.a)\n");
        KASSERT(curproc->p_state == PROC_RUNNING);
        dbg(DBG_PRINT, "(GRADING3A 7.a)\n");
        
        vmarea_t *vma, *clone_vma;
        pframe_t *pf;
        mmobj_t *to_delete, *new_shadowed;

        proc_t * new_proc = proc_create("new_proc");
        

        kthread_t *new_thread = kthread_clone(curthr);
        
        new_thread->kt_proc = new_proc;
        new_thread->kt_kstack = strcpy(new_thread->kt_kstack,curthr->kt_kstack);

        KASSERT(new_proc->p_state == PROC_RUNNING);
        dbg(DBG_PRINT, "(GRADING3A 7.a)\n");
        KASSERT(new_proc->p_pagedir != NULL);
        dbg(DBG_PRINT, "(GRADING3A 7.a)\n");
        KASSERT(new_thread->kt_kstack != NULL);
        dbg(DBG_PRINT, "(GRADING3A 7.a)\n");

        // add new_thread to new_proc's list of thread
        list_insert_tail(&new_proc->p_threads,&new_thread->kt_plink);
		
        new_proc->p_status = curproc->p_status;
        new_proc->p_state = curproc->p_state;

        new_proc->p_vmmap = vmmap_clone(curproc->p_vmmap);
        vmarea_t * cur_area;
        list_iterate_begin(&new_proc->p_vmmap->vmm_list, cur_area, vmarea_t, vma_plink){
                vmarea_t * orig_area = vmmap_lookup(curproc->p_vmmap,cur_area->vma_start);
                if(cur_area->vma_flags & MAP_PRIVATE){
                        mmobj_t * orig_obj = orig_area->vma_obj;
						orig_obj->mmo_ops->ref(orig_obj);

                        orig_area->vma_obj = shadow_create();
                        cur_area->vma_obj = shadow_create();
                        
                        orig_area->vma_obj->mmo_shadowed = orig_obj;
                        cur_area->vma_obj->mmo_shadowed = orig_obj;

                        orig_area->vma_obj->mmo_un.mmo_bottom_obj = mmobj_bottom_obj(orig_obj);
                        cur_area->vma_obj->mmo_un.mmo_bottom_obj = mmobj_bottom_obj(orig_obj);

                        orig_area->vma_obj->mmo_un.mmo_bottom_obj->mmo_ops->ref(orig_area->vma_obj->mmo_un.mmo_bottom_obj);
                        cur_area->vma_obj->mmo_un.mmo_bottom_obj->mmo_ops->ref(cur_area->vma_obj->mmo_un.mmo_bottom_obj);
                        dbg(DBG_PRINT, "(GRADING3D)\n");
                        
                        //list_insert_tail(mmobj_bottom_vmas(cur_area->vma_obj), &cur_area->vma_olink);
                }
                else{
                        cur_area->vma_obj = orig_area->vma_obj;
                        cur_area->vma_obj->mmo_ops->ref(cur_area->vma_obj);
                        dbg(DBG_PRINT, "(GRADING3D)\n");
                        //list_insert_tail(mmobj_bottom_vmas(cur_area->vma_obj), &cur_area->vma_olink);
                }
                dbg(DBG_PRINT, "(GRADING3D)\n");
        }list_iterate_end();

        new_proc->p_vmmap->vmm_proc = new_proc;

	pt_unmap_range(curproc->p_pagedir, USER_MEM_LOW, USER_MEM_HIGH);
        tlb_flush_all();

        regs->r_eax = 0;
        new_thread -> kt_ctx.c_pdptr = new_proc ->p_pagedir;
        new_thread -> kt_ctx.c_eip = (uint32_t) userland_entry;
        new_thread -> kt_ctx.c_esp = fork_setup_stack(regs,new_thread->kt_kstack);
        new_thread -> kt_ctx.c_kstack = (uintptr_t) new_thread ->kt_kstack;
        new_thread -> kt_ctx.c_kstacksz = DEFAULT_STACK_SIZE;

        // to do: retval

        for(int i = 0; i < NFILES; i++){
                new_proc->p_files[i] = curproc->p_files[i];
                if(new_proc->p_files[i]!=NULL){
                        fref(new_proc->p_files[i]);
                        dbg(DBG_PRINT, "(GRADING3D)\n");
                }
                dbg(DBG_PRINT, "(GRADING3D)\n");
        }

        
        
        
        new_proc->p_start_brk = curproc->p_start_brk;
        new_proc->p_brk = curproc->p_brk;

        new_proc->p_cwd = curproc->p_cwd;
        //vget(new_proc->p_cwd->vn_fs,new_proc->p_cwd->vn_vno);
        regs->r_eax = new_proc->p_pid;
        sched_make_runnable(new_thread);
        dbg(DBG_PRINT, "(GRADING3D)\n");
        return new_proc->p_pid;
}

