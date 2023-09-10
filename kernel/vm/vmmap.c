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

#include "kernel.h"
#include "errno.h"
#include "globals.h"

#include "vm/vmmap.h"
#include "vm/shadow.h"
#include "vm/anon.h"

#include "proc/proc.h"

#include "util/debug.h"
#include "util/list.h"
#include "util/string.h"
#include "util/printf.h"

#include "fs/vnode.h"
#include "fs/file.h"
#include "fs/fcntl.h"
#include "fs/vfs_syscall.h"

#include "mm/slab.h"
#include "mm/page.h"
#include "mm/mm.h"
#include "mm/mman.h"
#include "mm/mmobj.h"
#include "mm/tlb.h"

static slab_allocator_t *vmmap_allocator;
static slab_allocator_t *vmarea_allocator;

void
vmmap_init(void)
{
        vmmap_allocator = slab_allocator_create("vmmap", sizeof(vmmap_t));
        KASSERT(NULL != vmmap_allocator && "failed to create vmmap allocator!");
        vmarea_allocator = slab_allocator_create("vmarea", sizeof(vmarea_t));
        KASSERT(NULL != vmarea_allocator && "failed to create vmarea allocator!");
}

vmarea_t *
vmarea_alloc(void)
{
        vmarea_t *newvma = (vmarea_t *) slab_obj_alloc(vmarea_allocator);
        if (newvma) {
                newvma->vma_vmmap = NULL;
        }
        return newvma;
}

void
vmarea_free(vmarea_t *vma)
{
        KASSERT(NULL != vma);
        slab_obj_free(vmarea_allocator, vma);
}

/* a debugging routine: dumps the mappings of the given address space. */
size_t
vmmap_mapping_info(const void *vmmap, char *buf, size_t osize)
{
        KASSERT(0 < osize);
        KASSERT(NULL != buf);
        KASSERT(NULL != vmmap);

        vmmap_t *map = (vmmap_t *)vmmap;
        vmarea_t *vma;
        ssize_t size = (ssize_t)osize;

        int len = snprintf(buf, size, "%21s %5s %7s %8s %10s %12s\n",
                           "VADDR RANGE", "PROT", "FLAGS", "MMOBJ", "OFFSET",
                           "VFN RANGE");

        list_iterate_begin(&map->vmm_list, vma, vmarea_t, vma_plink) {
                size -= len;
                buf += len;
                if (0 >= size) {
                        goto end;
                }

                len = snprintf(buf, size,
                               "%#.8x-%#.8x  %c%c%c  %7s 0x%p %#.5x %#.5x-%#.5x\n",
                               vma->vma_start << PAGE_SHIFT,
                               vma->vma_end << PAGE_SHIFT,
                               (vma->vma_prot & PROT_READ ? 'r' : '-'),
                               (vma->vma_prot & PROT_WRITE ? 'w' : '-'),
                               (vma->vma_prot & PROT_EXEC ? 'x' : '-'),
                               (vma->vma_flags & MAP_SHARED ? " SHARED" : "PRIVATE"),
                               vma->vma_obj, vma->vma_off, vma->vma_start, vma->vma_end);
        } list_iterate_end();

end:
        if (size <= 0) {
                size = osize;
                buf[osize - 1] = '\0';
        }
        /*
        KASSERT(0 <= size);
        if (0 == size) {
                size++;
                buf--;
                buf[0] = '\0';
        }
        */
        return osize - size;
}

/* Create a new vmmap, which has no vmareas and does
 * not refer to a process. */
vmmap_t *
vmmap_create(void)
{
        vmmap_t * res = (vmmap_t *) slab_obj_alloc(vmmap_allocator);
        res->vmm_list.l_next = &res->vmm_list;
        res->vmm_list.l_prev = &res->vmm_list;
        res->vmm_proc = NULL;
        dbg(DBG_PRINT, "(GRADING3D)\n");
        return res;
}

/* Removes all vmareas from the address space and frees the
 * vmmap struct. */
void
vmmap_destroy(vmmap_t *map)
{
        KASSERT(NULL != map);
        dbg(DBG_PRINT, "(GRADING3A 3.a)\n");
        vmarea_t * cur_area;
        list_iterate_begin(&map->vmm_list, cur_area, vmarea_t, vma_plink){
                list_remove(&cur_area->vma_plink);
                list_remove(&cur_area->vma_olink);
                cur_area->vma_obj->mmo_ops->put(cur_area->vma_obj);
                vmarea_free(cur_area);
                dbg(DBG_PRINT, "(GRADING3D)\n");
        }list_iterate_end();

        slab_obj_free(vmmap_allocator, map);
        dbg(DBG_PRINT, "(GRADING3D)\n");
        return;
        //NOT_YET_IMPLEMENTED("VM: vmmap_destroy");
}

/* Add a vmarea to an address space. Assumes (i.e. asserts to some extent)
 * the vmarea is valid.  This involves finding where to put it in the list
 * of VM areas, and adding it. Don't forget to set the vma_vmmap for the
 * area. */
void
vmmap_insert(vmmap_t *map, vmarea_t *newvma)
{
        KASSERT(NULL != map && NULL != newvma);
        dbg(DBG_PRINT, "(GRADING3A 3.b)\n");
        KASSERT(NULL == newvma->vma_vmmap);
        dbg(DBG_PRINT, "(GRADING3A 3.b)\n");
        KASSERT(newvma->vma_start < newvma->vma_end); 
        dbg(DBG_PRINT, "(GRADING3A 3.b)\n");
        KASSERT(ADDR_TO_PN(USER_MEM_LOW) <= newvma->vma_start && ADDR_TO_PN(USER_MEM_HIGH) >= newvma->vma_end);
        dbg(DBG_PRINT, "(GRADING3A 3.b)\n");
        vmarea_t * cur_area;
        list_iterate_begin(&map->vmm_list, cur_area, vmarea_t, vma_plink){
                if(cur_area->vma_start >= newvma->vma_end){
                        list_insert_before(&cur_area->vma_plink,&newvma->vma_plink);
                        newvma->vma_vmmap = map;
                        dbg(DBG_PRINT, "(GRADING3D)\n");
                        return;
                }
                dbg(DBG_PRINT, "(GRADING3D)\n");
        }list_iterate_end();
        list_insert_tail(&map->vmm_list, &newvma->vma_plink);
        newvma->vma_vmmap = map;
        dbg(DBG_PRINT, "(GRADING3D)\n");
        return;
        //NOT_YET_IMPLEMENTED("VM: vmmap_insert");
}

/* Find a contiguous range of free virtual pages of length npages in
 * the given address space. Returns starting vfn for the range,
 * without altering the map. Returns -1 if no such range exists.
 *
 * Your algorithm should be first fit. If dir is VMMAP_DIR_HILO, you
 * should find a gap as high in the address space as possible; if dir
 * is VMMAP_DIR_LOHI, the gap should be as low as possible. */
int
vmmap_find_range(vmmap_t *map, uint32_t npages, int dir)
{
        vmarea_t * cur_area;
        // if(dir == VMMAP_DIR_LOHI){ // low to high address
        //         uint32_t lower_bound = ADDR_TO_PN(USER_MEM_LOW);
        //         list_iterate_begin(&map->vmm_list, cur_area, vmarea_t, vma_plink){
        //                 if(cur_area->vma_start - lower_bound >= npages){
        //                         dbg(DBG_PRINT, "vmmap_find_range_break1\n");
        //                         return lower_bound;
        //                 }
        //                 lower_bound = cur_area->vma_end;
        //                 dbg(DBG_PRINT, "vmmap_find_range_break2\n");
        //         }list_iterate_end();
        //         if(ADDR_TO_PN(USER_MEM_HIGH) - lower_bound >= npages){
        //                 dbg(DBG_PRINT, "vmmap_find_range_break3\n");
        //                 return lower_bound;
        //         }
        //         dbg(DBG_PRINT, "vmmap_find_range_break4\n");
        //         return -1;
        // }
        if(dir == VMMAP_DIR_HILO){ // high to low address
                uint32_t upper_bound = ADDR_TO_PN(USER_MEM_HIGH);
                list_iterate_reverse(&map->vmm_list, cur_area, vmarea_t, vma_plink){
                        if(upper_bound - cur_area->vma_end >= npages){
                                dbg(DBG_PRINT, "(GRADING3D)\n");
                                return upper_bound - npages;
                        }
                        dbg(DBG_PRINT, "(GRADING3D)\n");
                        upper_bound = cur_area->vma_start;
                }list_iterate_end();
                if(upper_bound - ADDR_TO_PN(USER_MEM_LOW) >= npages){
                        dbg(DBG_PRINT, "(GRADING3D)\n");
                        return upper_bound - npages;
                }
                dbg(DBG_PRINT, "(GRADING3D)\n");
                return -1;
        }return -1;
        //NOT_YET_IMPLEMENTED("VM: vmmap_find_range");
        //return -1;
}

/* Find the vm_area that vfn lies in. Simply scan the address space
 * looking for a vma whose range covers vfn. If the page is unmapped,
 * return NULL. */
vmarea_t *
vmmap_lookup(vmmap_t *map, uint32_t vfn)
{

        KASSERT(NULL != map);
        dbg(DBG_PRINT, "(GRADING3A 3.c)\n");

        vmarea_t * cur_area;
        list_iterate_begin(&map->vmm_list, cur_area, vmarea_t, vma_plink){
                if(vfn >= cur_area->vma_start && vfn < cur_area->vma_end){
                        dbg(DBG_PRINT, "(GRADING3D)\n");
                        return cur_area;
                }
                dbg(DBG_PRINT, "(GRADING3D)\n");
        } list_iterate_end();
        dbg(DBG_PRINT, "(GRADING3D)\n");
        //NOT_YET_IMPLEMENTED("VM: vmmap_lookup");
        return NULL;
}

/* Allocates a new vmmap containing a new vmarea for each area in the
 * given map. The areas should have no mmobjs set yet. Returns pointer
 * to the new vmmap on success, NULL on failure. This function is
 * called when implementing fork(2). */
vmmap_t *
vmmap_clone(vmmap_t *map)
{
        vmmap_t * new_map = vmmap_create();
        new_map->vmm_proc = map->vmm_proc;
        vmarea_t * cur_area;
        list_iterate_begin(&map->vmm_list, cur_area, vmarea_t, vma_plink){
                vmarea_t * new_area = vmarea_alloc();
                list_init(&new_area->vma_olink);
                list_init(&new_area->vma_plink);
                list_insert_tail(&new_map->vmm_list, &new_area->vma_plink);
                new_area->vma_start = cur_area->vma_start;
                new_area->vma_end = cur_area->vma_end;
                new_area->vma_flags = cur_area->vma_flags;
                new_area->vma_off = cur_area->vma_off;
                new_area->vma_prot = cur_area->vma_prot;
                new_area->vma_vmmap = new_map;
                new_area->vma_obj = NULL;
                dbg(DBG_PRINT, "(GRADING3D)\n");
                
        }list_iterate_end();
        dbg(DBG_PRINT, "(GRADING3D)\n");
        return new_map;
        
    /*    NOT_YET_IMPLEMENTED("VM: vmmap_clone");
        return NULL;*/
}

/* Insert a mapping into the map starting at lopage for npages pages.
 * If lopage is zero, we will find a range of virtual addresses in the
 * process that is big enough, by using vmmap_find_range with the same
 * dir argument.  If lopage is non-zero and the specified region
 * contains another mapping that mapping should be unmapped.
 *
 * If file is NULL an anon mmobj will be used to create a mapping
 * of 0's.  If file is non-null that vnode's file will be mapped in
 * for the given range.  Use the vnode's mmap operation to get the
 * mmobj for the file; do not assume it is file->vn_obj. Make sure all
 * of the area's fields except for vma_obj have been set before
 * calling mmap.
 *
 * If MAP_PRIVATE is specified set up a shadow object for the mmobj.
 *
 * All of the input to this function should be valid (KASSERT!).
 * See mmap(2) for for description of legal input.
 * Note that off should be page aligned.
 *
 * Be very careful about the order operations are performed in here. Some
 * operation are impossible to undo and should be saved until there
 * is no chance of failure.
 *
 * If 'new' is non-NULL a pointer to the new vmarea_t should be stored in it.
 */
int
vmmap_map(vmmap_t *map, vnode_t *file, uint32_t lopage, uint32_t npages,
          int prot, int flags, off_t off, int dir, vmarea_t **new)
{
        KASSERT(NULL != map); 
        dbg(DBG_PRINT, "(GRADING3A 3.d)\n");
        KASSERT(0 < npages);
        dbg(DBG_PRINT, "(GRADING3A 3.d)\n");
        KASSERT((MAP_SHARED & flags) || (MAP_PRIVATE & flags));
        dbg(DBG_PRINT, "(GRADING3A 3.d)\n");
        KASSERT((0 == lopage) || (ADDR_TO_PN(USER_MEM_LOW) <= lopage));
        dbg(DBG_PRINT, "(GRADING3A 3.d)\n");
        KASSERT((0 == lopage) || (ADDR_TO_PN(USER_MEM_HIGH) >= (lopage + npages)));
        dbg(DBG_PRINT, "(GRADING3A 3.d)\n");
        KASSERT(PAGE_ALIGNED(off));
        dbg(DBG_PRINT, "(GRADING3A 3.d)\n");

        vmarea_t * new_area = vmarea_alloc();
        new_area->vma_prot = prot;
        new_area->vma_flags = flags;
        list_init(&new_area->vma_plink);
        list_init(&new_area->vma_olink);
        if(lopage == 0){
                int start = vmmap_find_range(map,npages,dir);
                if(start < 0){
                        vmarea_free(new_area);
                        dbg(DBG_PRINT, "(GRADING3D)\n");
                        return start;
                }
                dbg(DBG_PRINT, "(GRADING3D)\n");
                new_area->vma_start = start;
        }
        else{
                vmmap_remove(map,lopage, npages);
                new_area->vma_start = lopage;
                dbg(DBG_PRINT, "(GRADING3D)\n");
        }
        new_area->vma_end = new_area->vma_start + npages;
        new_area->vma_off = ADDR_TO_PN(off);

        if(file == NULL){
                if(flags & MAP_PRIVATE){
                        new_area->vma_obj = shadow_create();
                        new_area->vma_obj->mmo_shadowed = anon_create();
                        new_area->vma_obj->mmo_un.mmo_bottom_obj = mmobj_bottom_obj(new_area->vma_obj->mmo_shadowed);
                        list_insert_tail(mmobj_bottom_vmas(new_area->vma_obj->mmo_shadowed), &new_area->vma_olink);
                        new_area->vma_obj->mmo_un.mmo_bottom_obj->mmo_ops->ref(new_area->vma_obj->mmo_un.mmo_bottom_obj);
                        dbg(DBG_PRINT, "(GRADING3D)\n");
                }
                else{
                        new_area->vma_obj = anon_create();
                        list_insert_tail(mmobj_bottom_vmas(new_area->vma_obj), &new_area->vma_olink);
                        dbg(DBG_PRINT, "(GRADING3D)\n");
                }
        }
        else{
                if(flags & MAP_PRIVATE){
                        new_area->vma_obj = shadow_create();
                        file->vn_ops->mmap(file, new_area, &new_area->vma_obj->mmo_shadowed);
                        new_area->vma_obj->mmo_un.mmo_bottom_obj = mmobj_bottom_obj(new_area->vma_obj->mmo_shadowed);
                        list_insert_tail(mmobj_bottom_vmas(new_area->vma_obj->mmo_shadowed), &new_area->vma_olink);
                        new_area->vma_obj->mmo_un.mmo_bottom_obj->mmo_ops->ref(new_area->vma_obj->mmo_un.mmo_bottom_obj);
                        dbg(DBG_PRINT, "(GRADING3D)\n");
                }
                else{
                        file->vn_ops->mmap(file, new_area, &new_area->vma_obj);
                        list_insert_tail(mmobj_bottom_vmas(new_area->vma_obj), &new_area->vma_olink);
                        //new_area->vma_obj->mmo_ops->ref(new_area->vma_obj);
                        dbg(DBG_PRINT, "(GRADING3D)\n");
                }

        }
        
        vmmap_insert(map, new_area);

        if(new != NULL){
                *new = new_area;
                dbg(DBG_PRINT, "(GRADING3D)\n");
        }
        dbg(DBG_PRINT, "(GRADING3D)\n");
        return 0;
        //NOT_YET_IMPLEMENTED("VM: vmmap_map");
        //return -1;
}

/*
 * We have no guarantee that the region of the address space being
 * unmapped will play nicely with our list of vmareas.
 *
 * You must iterate over each vmarea that is partially or wholly covered
 * by the address range [addr ... addr+len). The vm-area will fall into one
 * of four cases, as illustrated below:
 *
 * key:
 *          [             ]   Existing VM Area
 *        *******             Region to be unmapped
 *
 * Case 1:  [   ******    ]
 * The region to be unmapped lies completely inside the vmarea. We need to
 * split the old vmarea into two vmareas. be sure to increment the
 * reference count to the file associated with the vmarea.
 *
 * Case 2:  [      *******]**
 * The region overlaps the end of the vmarea. Just shorten the length of
 * the mapping.
 *
 * Case 3: *[*****        ]
 * The region overlaps the beginning of the vmarea. Move the beginning of
 * the mapping (remember to update vma_off), and shorten its length.
 *
 * Case 4: *[*************]**
 * The region completely contains the vmarea. Remove the vmarea from the
 * list.
 */
int
vmmap_remove(vmmap_t *map, uint32_t lopage, uint32_t npages)
{       
        // if(npages == 0){
        //         return 0;
        //         dbg(DBG_PRINT, "vmmap_remove_break1\n");
        // }
        vmarea_t * cur_area;
        uint32_t hipage = lopage + npages;
        list_iterate_begin(&map->vmm_list, cur_area, vmarea_t, vma_plink){
                if(lopage > cur_area->vma_start && hipage < cur_area->vma_end){
                        vmarea_t * new_area = vmarea_alloc();
                        list_init(&new_area->vma_olink);
                        list_init(&new_area->vma_plink);
                        
                        new_area->vma_start = cur_area->vma_start;
                        new_area->vma_end = lopage;
                        new_area->vma_flags = cur_area->vma_flags;
                        new_area->vma_off = cur_area->vma_off;
                        new_area->vma_prot = cur_area->vma_prot;
                        new_area->vma_vmmap = map;
                        new_area->vma_obj = cur_area->vma_obj;
                        new_area->vma_obj->mmo_ops->ref(new_area->vma_obj);
                        cur_area->vma_off = hipage - cur_area->vma_start + cur_area->vma_off;
                        cur_area->vma_start = hipage;

                        list_insert_before(&(cur_area->vma_plink),&(new_area->vma_plink));     
                        list_insert_before(&(cur_area->vma_olink),&(new_area->vma_olink));
                         
                        pt_unmap_range(curproc->p_pagedir, (uintptr_t)PN_TO_ADDR(lopage), (uintptr_t)PN_TO_ADDR(hipage));
                        dbg(DBG_PRINT, "(GRADING3D)\n");
                }
                else if(lopage > cur_area->vma_start && lopage < cur_area->vma_end && hipage >= cur_area->vma_end){
                        pt_unmap_range(curproc->p_pagedir, (uintptr_t)PN_TO_ADDR(lopage), (uintptr_t)PN_TO_ADDR(cur_area->vma_end));
                        cur_area->vma_end = lopage;
                        dbg(DBG_PRINT, "(GRADING3D)\n");
                }
                else if(lopage <= cur_area->vma_start && hipage > cur_area->vma_start && hipage < cur_area->vma_end){
                        pt_unmap_range(curproc->p_pagedir, (uintptr_t)PN_TO_ADDR(cur_area->vma_start), (uintptr_t)PN_TO_ADDR(hipage));
                        cur_area->vma_off = hipage - cur_area->vma_start + cur_area->vma_off;
                        cur_area->vma_start = hipage;
                        dbg(DBG_PRINT, "(GRADING3D)\n");
                }
                else if(lopage <= cur_area->vma_start && hipage >= cur_area->vma_end){
                        pt_unmap_range(curproc->p_pagedir, (uintptr_t)PN_TO_ADDR(cur_area->vma_start), (uintptr_t)PN_TO_ADDR(cur_area->vma_end));
                        cur_area->vma_obj->mmo_ops->put(cur_area->vma_obj);
                        
                        list_remove(&cur_area->vma_olink);
                        list_remove(&cur_area->vma_plink);
                        vmarea_free(cur_area);
                        dbg(DBG_PRINT, "(GRADING3D)\n");
                }
                dbg(DBG_PRINT, "(GRADING3D)\n");
        }list_iterate_end();
        tlb_flush_all();
        dbg(DBG_PRINT, "(GRADING3D)\n");
        
        return 0;
}

/*
 * Returns 1 if the given address space has no mappings for the
 * given range, 0 otherwise.
 */
int
vmmap_is_range_empty(vmmap_t *map, uint32_t startvfn, uint32_t npages)
{
        uint32_t endvfn = startvfn+npages;
        KASSERT((startvfn < endvfn) && (ADDR_TO_PN(USER_MEM_LOW) <= startvfn) && (ADDR_TO_PN(USER_MEM_HIGH) >= endvfn));
        dbg(DBG_PRINT, "(GRADING3A 3.e)\n");

        vmarea_t * cur_area;
        uint32_t lower_bound = ADDR_TO_PN(USER_MEM_LOW);
        list_iterate_begin(&map->vmm_list, cur_area, vmarea_t, vma_plink){
                if(lower_bound <= startvfn && cur_area->vma_start >= startvfn+npages){
                        dbg(DBG_PRINT, "(GRADING3D)\n");
                        return 1;
                }
                dbg(DBG_PRINT, "(GRADING3D)\n");
                lower_bound = cur_area->vma_end;
        }list_iterate_end();
        if(lower_bound <= startvfn && ADDR_TO_PN(USER_MEM_HIGH) >= startvfn+npages){
                dbg(DBG_PRINT, "(GRADING3D)\n");
                return 1;
        }
        dbg(DBG_PRINT, "(GRADING3D)\n");
        //NOT_YET_IMPLEMENTED("VM: vmmap_is_range_empty");
        return 0;
}

/* Read into 'buf' from the virtual address space of 'map' starting at
 * 'vaddr' for size 'count'. To do so, you will want to find the vmareas
 * to read from, then find the pframes within those vmareas corresponding
 * to the virtual addresses you want to read, and then read from the
 * physical memory that pframe points to. You should not check permissions
 * of the areas. Assume (KASSERT) that all the areas you are accessing exist.
 * Returns 0 on success, -errno on error.
 */
int
vmmap_read(vmmap_t *map, const void *vaddr, void *buf, size_t count)
{
        int cur_addr = 0 + (int) vaddr; 
        uint32_t cur_page = ADDR_TO_PN(cur_addr);
        vmarea_t * cur_area;
        list_iterate_begin(&map->vmm_list, cur_area, vmarea_t, vma_plink){
                while(cur_area->vma_start <= cur_page && cur_area->vma_end > cur_page){
                        pframe_t * cur_frame;
                        int ret = pframe_lookup(cur_area->vma_obj, cur_page - cur_area->vma_start + cur_area->vma_off, 0, &cur_frame);
                        if(ret < 0){
                                dbg(DBG_PRINT, "(GRADING3D)\n");
                                return ret;
                        }
                        size_t page_offset = PAGE_OFFSET(cur_addr);
                        size_t read = MIN(PAGE_SIZE-page_offset, (int)vaddr + count - cur_addr);
                        memcpy(buf, (const void*)((int)cur_frame->pf_addr + page_offset), read);
                        cur_addr += read;
                        buf = (void*)((int)buf + read);
                        cur_page = ADDR_TO_PN(cur_addr);
                        if(cur_addr == (int)vaddr + (int)count){
                                dbg(DBG_PRINT, "(GRADING3D)\n");
                                return 0;
                        }
                        dbg(DBG_PRINT, "(GRADING3D)\n");
                }
                dbg(DBG_PRINT, "(GRADING3D)\n");
                
        }list_iterate_end();return -1;
}

/* Write from 'buf' into the virtual address space of 'map' starting at
 * 'vaddr' for size 'count'. To do this, you will need to find the correct
 * vmareas to write into, then find the correct pframes within those vmareas,
 * and finally write into the physical addresses that those pframes correspond
 * to. You should not check permissions of the areas you use. Assume (KASSERT)
 * that all the areas you are accessing exist. Remember to dirty pages!
 * Returns 0 on success, -errno on error.
 */
int
vmmap_write(vmmap_t *map, void *vaddr, const void *buf, size_t count)
{
        void * cur_addr = vaddr; 
        void * buf_addr = (void*)(0 + (int)buf);
        uint32_t cur_page = ADDR_TO_PN(cur_addr);
        vmarea_t * cur_area;
        list_iterate_begin(&map->vmm_list, cur_area, vmarea_t, vma_plink){
                while(cur_area->vma_start <= cur_page && cur_area->vma_end > cur_page){
                        pframe_t * cur_frame;
                        int ret = pframe_lookup(cur_area->vma_obj, cur_page - cur_area->vma_start + cur_area->vma_off, 1, &cur_frame);
                        // if(ret < 0){
                        //         dbg(DBG_PRINT, "vmmap_write_break1\n");
                        //         return ret;
                        // }
                        size_t page_offset = PAGE_OFFSET(cur_addr);
                        size_t write = MIN(PAGE_SIZE - page_offset, (int)vaddr + count - (int)cur_addr);
                        pframe_dirty(cur_frame);
                        memcpy((void*)((int)cur_frame->pf_addr + page_offset), buf_addr, write);
                        cur_addr = (void*)((int)cur_addr + write);
                        buf_addr = (void*)((int)buf_addr + write);
                        cur_page = ADDR_TO_PN(cur_addr);
                        if((int)buf_addr == (int)buf + (int)count){
                                dbg(DBG_PRINT, "(GRADING3D)\n");
                                return 0;
                        }
                        dbg(DBG_PRINT, "(GRADING3D)\n");
                }
                dbg(DBG_PRINT, "(GRADING3D)\n");
        }list_iterate_end();return -1;
}
