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

#include "util/string.h"
#include "util/debug.h"

#include "mm/mmobj.h"
#include "mm/pframe.h"
#include "mm/mm.h"
#include "mm/page.h"
#include "mm/slab.h"
#include "mm/tlb.h"

#include "vm/vmmap.h"
#include "vm/shadow.h"
#include "vm/shadowd.h"

#define SHADOW_SINGLETON_THRESHOLD 5

int shadow_count = 0; /* for debugging/verification purposes */
#ifdef __SHADOWD__
/*
 * number of shadow objects with a single parent, that is another shadow
 * object in the shadow objects tree(singletons)
 */
static int shadow_singleton_count = 0;
#endif

static slab_allocator_t *shadow_allocator;

static void shadow_ref(mmobj_t *o);
static void shadow_put(mmobj_t *o);
static int  shadow_lookuppage(mmobj_t *o, uint32_t pagenum, int forwrite, pframe_t **pf);
static int  shadow_fillpage(mmobj_t *o, pframe_t *pf);
static int  shadow_dirtypage(mmobj_t *o, pframe_t *pf);
static int  shadow_cleanpage(mmobj_t *o, pframe_t *pf);

static mmobj_ops_t shadow_mmobj_ops = {
        .ref = shadow_ref,
        .put = shadow_put,
        .lookuppage = shadow_lookuppage,
        .fillpage  = shadow_fillpage,
        .dirtypage = shadow_dirtypage,
        .cleanpage = shadow_cleanpage
};

/*
 * This function is called at boot time to initialize the
 * shadow page sub system. Currently it only initializes the
 * shadow_allocator object.
 */
void
shadow_init()
{
        shadow_allocator = slab_allocator_create("shadow",sizeof(mmobj_t));
        KASSERT(shadow_allocator);
        dbg(DBG_PRINT, "(GRADING3A 6.a)\n");
        dbg(DBG_PRINT, "(GRADING3D)\n");
}

/*
 * You'll want to use the shadow_allocator to allocate the mmobj to
 * return, then then initialize it. Take a look in mm/mmobj.h for
 * macros or functions which can be of use here. Make sure your initial
 * reference count is correct.
 */
mmobj_t *
shadow_create()
{
        mmobj_t* res = (mmobj_t*)slab_obj_alloc(shadow_allocator);

        mmobj_init(res,&shadow_mmobj_ops);
        res->mmo_refcount = 1;

        dbg(DBG_PRINT, "(GRADING3D)\n");
        return res;
}

/* Implementation of mmobj entry points: */

/*
 * Increment the reference count on the object.
 */
static void
shadow_ref(mmobj_t *o)
{
        KASSERT(o && (0 < o->mmo_refcount) && (&shadow_mmobj_ops == o->mmo_ops));
        dbg(DBG_PRINT, "(GRADING3A 6.b)\n");

        o->mmo_refcount++;
        dbg(DBG_PRINT, "(GRADING3D)\n");
}

/*
 * Decrement the reference count on the object. If, however, the
 * reference count on the object reaches the number of resident
 * pages of the object, we can conclude that the object is no
 * longer in use and, since it is a shadow object, it will never
 * be used again. You should unpin and uncache all of the object's
 * pages and then free the object itself.
 */
static void
shadow_put(mmobj_t *o)
{
        KASSERT(o && (0 < o->mmo_refcount) && (&shadow_mmobj_ops == o->mmo_ops));
        dbg(DBG_PRINT, "(GRADING3A 6.c)\n");
        
        pframe_t *temp;

        if(o->mmo_refcount-1==o->mmo_nrespages){
                // unpin and uncache, then free
                list_iterate_begin(&o->mmo_respages,temp,pframe_t,pf_olink){
                        pframe_unpin(temp);
                        pframe_free(temp);
                        dbg(DBG_PRINT, "(GRADING3D)\n");
                }list_iterate_end();
                dbg(DBG_PRINT, "(GRADING3D)\n");

                o->mmo_shadowed->mmo_ops->put(o->mmo_shadowed);
                o->mmo_un.mmo_bottom_obj->mmo_ops->put(o->mmo_un.mmo_bottom_obj);
                o->mmo_refcount--;
                slab_obj_free(shadow_allocator,o);
                return;
        }
    
        o->mmo_refcount--;
        dbg(DBG_PRINT, "(GRADING3D)\n");
        
}

/* This function looks up the given page in this shadow object. The
 * forwrite argument is true if the page is being looked up for
 * writing, false if it is being looked up for reading. This function
 * must handle all do-not-copy-on-not-write magic (i.e. when forwrite
 * is false find the first shadow object in the chain which has the
 * given page resident). copy-on-write magic (necessary when forwrite
 * is true) is handled in shadow_fillpage, not here. It is important to
 * use iteration rather than recursion here as a recursive implementation
 * can overflow the kernel stack when looking down a long shadow chain */
static int
shadow_lookuppage(mmobj_t *o, uint32_t pagenum, int forwrite, pframe_t **pf)
{
        pframe_t *p = NULL;
        mmobj_t *cur = o;
        int flag = 0;
        if(forwrite==1){
                // for writing
                while(cur->mmo_shadowed!=NULL){
                        p = pframe_get_resident(cur,pagenum);
                        if(p!=NULL){
                                if(flag > 0){
                                        pframe_get(o, p -> pf_pagenum, pf);
                                        pframe_set_busy(*pf);
                                        shadow_fillpage(o,*pf);
                                        pframe_clear_busy(*pf);
                                        KASSERT(NULL != (*pf));
                                        dbg(DBG_PRINT, "(GRADING3A 6.d)\n");
                                        KASSERT((pagenum == (*pf)->pf_pagenum) && (!pframe_is_busy(*pf)));
                                        dbg(DBG_PRINT, "(GRADING3A 6.d)\n");
                                        return 0;
                                }
                                dbg(DBG_PRINT, "(GRADING3D)\n");
                                break;
                        }
                        flag++;
                        cur = cur->mmo_shadowed;
                        dbg(DBG_PRINT, "(GRADING3D)\n");
                }
                if(((p==NULL))){
                        int err = cur->mmo_ops->lookuppage(cur,pagenum,forwrite,&p);
                        if(err<0){
                                return err;
                        }
                        if(flag > 0){
                                pframe_get(o, p -> pf_pagenum, pf);
                                pframe_set_busy(*pf);
                                shadow_fillpage(o,*pf);
                                pframe_clear_busy(*pf);
                                KASSERT(NULL != (*pf));
                                dbg(DBG_PRINT, "(GRADING3A 6.d)\n");
                                KASSERT((pagenum == (*pf)->pf_pagenum) && (!pframe_is_busy(*pf)));
                                dbg(DBG_PRINT, "(GRADING3A 6.d)\n");
                                return 0;
                        }
                }
                

                dbg(DBG_PRINT, "(GRADING3D)\n");
                *pf = p;
                KASSERT(NULL != (*pf));
                dbg(DBG_PRINT, "(GRADING3A 6.d)\n");
                KASSERT((pagenum == (*pf)->pf_pagenum) && (!pframe_is_busy(*pf)));
                dbg(DBG_PRINT, "(GRADING3A 6.d)\n");
                return 0;
        } 
        else {
                while(cur->mmo_shadowed!=NULL){
                        p = pframe_get_resident(cur,pagenum);
                        if(p!=NULL){
                                dbg(DBG_PRINT, "(GRADING3D)\n");
                                *pf = p;
                                KASSERT(NULL != (*pf));
                                dbg(DBG_PRINT, "(GRADING3A 6.d)\n");
                                KASSERT((pagenum == (*pf)->pf_pagenum) && (!pframe_is_busy(*pf)));
                                dbg(DBG_PRINT, "(GRADING3A 6.d)\n");
                                return 0;//break;
                        }
                        cur = cur->mmo_shadowed;
                        dbg(DBG_PRINT, "(GRADING3D)\n");
                }
                if(p==NULL ){
                        int err = cur->mmo_ops->lookuppage(cur,pagenum,forwrite,&p);
                        if(err<0){
                                return err;
                        }
                }
                *pf=p;

        }
    
        
        KASSERT(NULL != (*pf));
        dbg(DBG_PRINT, "(GRADING3A 6.d)\n");
        KASSERT((pagenum == (*pf)->pf_pagenum) && (!pframe_is_busy(*pf)));
        dbg(DBG_PRINT, "(GRADING3A 6.d)\n");

        return 0;
}

/* As per the specification in mmobj.h, fill the page frame starting
 * at address pf->pf_addr with the contents of the page identified by
 * pf->pf_obj and pf->pf_pagenum. This function handles all
 * copy-on-write magic (i.e. if there is a shadow object which has
 * data for the pf->pf_pagenum-th page then we should take that data,
 * if no such shadow object exists we need to follow the chain of
 * shadow objects all the way to the bottom object and take the data
 * for the pf->pf_pagenum-th page from the last object in the chain).
 * It is important to use iteration rather than recursion here as a
 * recursive implementation can overflow the kernel stack when
 * looking down a long shadow chain */
static int
shadow_fillpage(mmobj_t *o, pframe_t *pf)
{
        KASSERT(pframe_is_busy(pf));
        dbg(DBG_PRINT, "(GRADING3A 6.e)\n");
        KASSERT(!pframe_is_pinned(pf));
        dbg(DBG_PRINT, "(GRADING3A 6.e)\n");

        mmobj_t *cur = o;
        pframe_t *p;

        int lookupRes = pframe_lookup(o->mmo_shadowed,pf->pf_pagenum,0,&p);
    
        if(lookupRes<0){
                dbg(DBG_PRINT, "(GRADING3D)\n");
                return lookupRes;
        }

        pframe_pin(pf);
        memcpy(pf->pf_addr, p->pf_addr, PAGE_SIZE);
        dbg(DBG_PRINT, "(GRADING3D)\n");
        return 0;

}

/* These next two functions are not difficult. */

static int
shadow_dirtypage(mmobj_t *o, pframe_t *pf)
{
        pframe_set_dirty(pf);
        dbg(DBG_PRINT, "(GRADING3D)\n");
        return 0;
}

static int
shadow_cleanpage(mmobj_t *o, pframe_t *pf)
{
        // pframe_t  *p;
        // int res = shadow_lookuppage(o->mmo_shadowed,pf->pf_pagenum,0,&p);

        // if(p==NULL){
        //         dbg(DBG_PRINT, "(GRADING3D)\n");
        //         return res;
        // }
        
        // memcpy(p->pf_addr,pf->pf_addr,PAGE_SIZE);
        // pframe_clear_dirty(p);
        // dbg(DBG_PRINT, "(GRADING3D)\n");
        return 0;
}
