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

/*
 *  FILE: vfs_syscall.c
 *  AUTH: mcc | jal
 *  DESC:
 *  DATE: Wed Apr  8 02:46:19 1998
 *  $Id: vfs_syscall.c,v 1.2 2018/05/27 03:57:26 cvsps Exp $
 */

#include "kernel.h"
#include "errno.h"
#include "globals.h"
#include "fs/vfs.h"
#include "fs/file.h"
#include "fs/vnode.h"
#include "fs/vfs_syscall.h"
#include "fs/open.h"
#include "fs/fcntl.h"
#include "fs/lseek.h"
#include "mm/kmalloc.h"
#include "util/string.h"
#include "util/printf.h"
#include "fs/stat.h"
#include "util/debug.h"

/*
 * Syscalls for vfs. Refer to comments or man pages for implementation.
 * Do note that you don't need to set errno, you should just return the
 * negative error code.
 */

/* To read a file:
 *      o fget(fd)
 *      o call its virtual read vn_op
 *      o update f_pos
 *      o fput() it
 *      o return the number of bytes read, or an error
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        fd is not a valid file descriptor or is not open for reading.
 *      o EISDIR
 *        fd refers to a directory.
 *
 * In all cases, be sure you do not leak file refcounts by returning before
 * you fput() a file that you fget()'ed.
 */
int
do_read(int fd, void *buf, size_t nbytes)
{
        if(fd<0 || fd>=NFILES){
                dbg(DBG_PRINT, "(GRADING2B)\n");
                return -EBADF;
        }

        file_t *file = fget(fd);

        if(file==NULL){
                dbg(DBG_PRINT, "(GRADING2B)\n");
                return -EBADF;
        }
	
	

        if(!(file->f_mode & FMODE_READ)){
                fput(file);
                dbg(DBG_PRINT, "(GRADING2B)\n");
                return -EBADF;
        }
	
	if(S_ISDIR(file->f_vnode->vn_mode)){
                fput(file);
                dbg(DBG_PRINT, "(GRADING2B)\n");
                return -EISDIR;
        }
        

        int count = file->f_vnode->vn_ops->read(file->f_vnode,file->f_pos,buf,nbytes);
        file->f_pos+=count;
        
        fput(file);
        dbg(DBG_PRINT, "(GRADING2B)\n");
        return count;

}

/* Very similar to do_read.  Check f_mode to be sure the file is writable.  If
 * f_mode & FMODE_APPEND, do_lseek() to the end of the file, call the write
 * vn_op, and fput the file.  As always, be mindful of refcount leaks.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        fd is not a valid file descriptor or is not open for writing.
 */
int
do_write(int fd, const void *buf, size_t nbytes)
{
        // check if fd is valid
        if(fd<0 || fd>=NFILES|| curproc->p_files[fd] == NULL){
                dbg(DBG_PRINT, "(GRADING2B)\n");
                return -EBADF;
        }

        file_t *file = fget(fd);



	       
	if(file==NULL){
                dbg(DBG_PRINT, "write break 2\n");
                return -EBADF;
        }
	
        if(!(file->f_mode & FMODE_WRITE)){
                fput(file);
                dbg(DBG_PRINT, "(GRADING2B)\n");
                return -EBADF;
        }

        if(file->f_mode & FMODE_APPEND){
                file->f_pos = do_lseek(fd,0,SEEK_END);
                dbg(DBG_PRINT, "(GRADING2B)\n");
        }

	
        int count = file->f_vnode->vn_ops->write(file->f_vnode,file->f_pos,buf,nbytes);

	
		//fput(file);
		//dbg(DBG_PRINT, "(GRADING2B)\n");
		//return count;
	

        file->f_pos+=count;


        KASSERT((S_ISCHR(file->f_vnode->vn_mode)) ||(S_ISBLK(file->f_vnode->vn_mode)) || ((S_ISREG(file->f_vnode->vn_mode)) && (file->f_pos <= file->f_vnode->vn_len)));
        dbg(DBG_PRINT, "(GRADING2A 3.a)\n");
        fput(file);
        
        dbg(DBG_PRINT, "(GRADING2B)\n");
        return count;
}

/*
 * Zero curproc->p_files[fd], and fput() the file. Return 0 on success
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        fd isn't a valid open file descriptor.
 */
int
do_close(int fd)
{
        if(fd<0 || fd>=NFILES || curproc->p_files[fd]==NULL){
                dbg(DBG_PRINT, "(GRADING2B)\n");
                return -EBADF;
        }

        fput(curproc->p_files[fd]);
        curproc->p_files[fd]=NULL;

        dbg(DBG_PRINT, "(GRADING2B)\n");
        return 0;
}

/* To dup a file:
 *      o fget(fd) to up fd's refcount
 *      o get_empty_fd()
 *      o point the new fd to the same file_t* as the given fd
 *      o return the new file descriptor
 *
 * Don't fput() the fd unless something goes wrong.  Since we are creating
 * another reference to the file_t*, we want to up the refcount.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        fd isn't an open file descriptor.
 *      o EMFILE
 *        The process already has the maximum number of file descriptors open
 *        and tried to open a new one.
 */
int
do_dup(int fd)
{
        if(fd<0 || fd>=NFILES || curproc->p_files[fd]==NULL){
                dbg(DBG_PRINT, "(GRADING2B)\n");
                return -EBADF;
        }
        
        file_t *file = fget(fd);

	        
	if(file==NULL){
                dbg(DBG_PRINT, "dup break 2\n");
                return -EBADF;
        }
	

        // -EMFILE if the process has the max num of file descriptoers open and tried to open a new one
        int newFd = get_empty_fd(curproc);
	        
	if(newFd<0){
		fput(file);
		dbg(DBG_PRINT, "dup break 3\n");
                return newFd;
        }
	
        curproc->p_files[newFd] = file;
	
        dbg(DBG_PRINT, "(GRADING2B)\n");
        return newFd;
}

/* Same as do_dup, but insted of using get_empty_fd() to get the new fd,
 * they give it to us in 'nfd'.  If nfd is in use (and not the same as ofd)
 * do_close() it first.  Then return the new file descriptor.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        ofd isn't an open file descriptor, or nfd is out of the allowed
 *        range for file descriptors.
 */
int
do_dup2(int ofd, int nfd)
{
        if(ofd<0 || ofd>=NFILES || curproc->p_files[ofd]==NULL){
                dbg(DBG_PRINT, "(GRADING2B)\n");
                return -EBADF;
        }
	
	
        if(nfd<0 || nfd>=NFILES){
                dbg(DBG_PRINT, "dup2 break 2\n");
                return -EBADF;
        }
	
	

        file_t *file = fget(ofd);
	
	
        if(file==NULL){
                dbg(DBG_PRINT, "dup2 break 3\n");
                return -EBADF;
        }

        if(curproc->p_files[nfd]!=NULL && nfd!=ofd){
                dbg(DBG_PRINT, "dup2 break 4\n");
                do_close(nfd);
        }
	
	if(nfd == ofd){
		fput(file);
		dbg(DBG_PRINT, "(GRADING2B)\n");
		return nfd;
	}

        curproc->p_files[nfd]=file;
        
        dbg(DBG_PRINT, "(GRADING2B)\n");
        return nfd;
}

/*
 * This routine creates a special file of the type specified by 'mode' at
 * the location specified by 'path'. 'mode' should be one of S_IFCHR or
 * S_IFBLK (you might note that mknod(2) normally allows one to create
 * regular files as well-- for simplicity this is not the case in Weenix).
 * 'devid', as you might expect, is the device identifier of the device
 * that the new special file should represent.
 *
 * You might use a combination of dir_namev, lookup, and the fs-specific
 * mknod (that is, the containing directory's 'mknod' vnode operation).
 * Return the result of the fs-specific mknod, or an error.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EINVAL
 *        mode requested creation of something other than a device special
 *        file.
 *      o EEXIST
 *        path already exists.
 *      o ENOENT
 *        A directory component in path does not exist.
 *      o ENOTDIR
 *        A component used as a directory in path is not, in fact, a directory.
 *      o ENAMETOOLONG
 *        A component of path was too long.
 */
int
do_mknod(const char *path, int mode, unsigned devid)
{

        if(mode!=S_IFCHR && mode!=S_IFBLK){
                dbg(DBG_PRINT, "(GRADING2B)\n");
                return -EINVAL;
        }
        

        size_t len=0;
        const char *name = NULL;
        vnode_t * dir = NULL;

        int errVal = dir_namev(path,&len,&name,NULL,&dir);
	
	
        if(errVal<0){
                dbg(DBG_PRINT, "mknod break 2\n");
                return errVal;
        }
	
	


        vnode_t *lookupRes = NULL;

        int errLookUp = lookup(dir,name,len,&lookupRes);
	
	if(errLookUp== -ENOENT){
                KASSERT(NULL != dir->vn_ops->mknod);
                dbg(DBG_PRINT, "(GRADING2A 3.b)\n");
                int res = dir->vn_ops->mknod(dir,name,len,mode,(devid_t)devid);
                vput(dir);

                dbg(DBG_PRINT, "(GRADING2B)\n");
                return res;
        }//return 0;
	
	
        if(errLookUp==0){
                vput(lookupRes);
		vput(dir);
                dbg(DBG_PRINT, "mknod break 3\n");
                return -EEXIST;
        } else if(errLookUp== -ENOENT){
                KASSERT(NULL != dir->vn_ops->mknod);
                dbg(DBG_PRINT, "(GRADING2A 3.b)\n");
                int res = dir->vn_ops->mknod(dir,name,len,mode,(devid_t)devid);
                vput(dir);

                dbg(DBG_PRINT, "(GRADING2B)\n");
                return res;
        } else {
                vput(dir);
                dbg(DBG_PRINT, "mknod break 5\n");
                return errLookUp;
        }
	
}

/* Use dir_namev() to find the vnode of the dir we want to make the new
 * directory in.  Then use lookup() to make sure it doesn't already exist.
 * Finally call the dir's mkdir vn_ops. Return what it returns.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EEXIST
 *        path already exists.
 *      o ENOENT
 *        A directory component in path does not exist.
 *      o ENOTDIR
 *        A component used as a directory in path is not, in fact, a directory.
 *      o ENAMETOOLONG
 *        A component of path was too long.
 */
int
do_mkdir(const char *path)
{
        const char* name=NULL;
        size_t len=0;
        vnode_t *dir=NULL;

        int errVal = dir_namev(path,&len,&name,NULL,&dir);

        if(errVal<0){
                dbg(DBG_PRINT, "(GRADING2B)\n");
                return errVal;
        }

        vnode_t *lookupRes=NULL;

        int errLookUp = lookup(dir,name,len,&lookupRes);


        if(errLookUp==0){
                vput(dir);
                vput(lookupRes);

                dbg(DBG_PRINT, "(GRADING2B)\n");
                return -EEXIST;
        } else if(errLookUp==-ENOENT){
                vput(dir);
                KASSERT(NULL != dir->vn_ops->mkdir);
                dbg(DBG_PRINT, "(GRADING2A 3.c)\n");

                int res = dir->vn_ops->mkdir(dir,name,len);

                dbg(DBG_PRINT, "(GRADING2B)\n");
                return res;
        } else {
                vput(dir);

                dbg(DBG_PRINT, "(GRADING2B)\n");
                return errLookUp;
        }
}

/* Use dir_namev() to find the vnode of the directory containing the dir to be
 * removed. Then call the containing dir's rmdir v_op.  The rmdir v_op will
 * return an error if the dir to be removed does not exist or is not empty, so
 * you don't need to worry about that here. Return the value of the v_op,
 * or an error.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EINVAL
 *        path has "." as its final component.
 *      o ENOTEMPTY
 *        path has ".." as its final component.
 *      o ENOENT
 *        A directory component in path does not exist.
 *      o ENOTDIR
 *        A component used as a directory in path is not, in fact, a directory.
 *      o ENAMETOOLONG
 *        A component of path was too long.
 */
int
do_rmdir(const char *path)
{
        const char* name=NULL;
        size_t len=0;
        vnode_t *dir=NULL;

        int errVal = dir_namev(path,&len,&name,NULL,&dir);

        if(errVal<0){
                dbg(DBG_PRINT, "(GRADING2B)\n");
                return errVal;
        }

        if(name_match(".",name,len)){
                vput(dir);

                dbg(DBG_PRINT, "(GRADING2B)\n");
                return -EINVAL;
        }

        if(name_match("..",name,len)){
                vput(dir);

                dbg(DBG_PRINT, "(GRADING2B)\n");
                return -ENOTEMPTY;
        }

        KASSERT(NULL != dir->vn_ops->rmdir);
        dbg(DBG_PRINT, "(GRADING2A 3.d)\n");

        int errRem = dir->vn_ops->rmdir(dir,name,len);
        vput(dir);
	dbg(DBG_PRINT, "(GRADING2B)\n");
        return errRem;
}

/*
 * Similar to do_rmdir, but for files.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EPERM
 *        path refers to a directory.
 *      o ENOENT
 *        Any component in path does not exist, including the element at the
 *        very end.
 *      o ENOTDIR
 *        A component used as a directory in path is not, in fact, a directory.
 *      o ENAMETOOLONG
 *        A component of path was too long.
 */
int
do_unlink(const char *path)
{
        if(strlen(path) > MAXPATHLEN){
		dbg(DBG_PRINT, "unlink break 1\n");
                return -ENAMETOOLONG;
        }

        size_t namelen;
        const char * name; 
        vnode_t * dir_vnode = NULL;
        int error = dir_namev(path, &namelen, &name, NULL, &dir_vnode);

        if(error < 0){
		dbg(DBG_PRINT, "(GRADING2B)\n");
                return error;
        }

        vnode_t * file_vnode = NULL;
        error = lookup(dir_vnode, name, namelen, &file_vnode);

        if(error < 0){
		dbg(DBG_PRINT, "(GRADING2B)\n");
                if(dir_vnode != NULL){
			dbg(DBG_PRINT, "(GRADING2B)\n");
                        vput(dir_vnode);
                }
                return error;
        }
        if(file_vnode->vn_mode == S_IFDIR){
		dbg(DBG_PRINT, "(GRADING2B)\n");
                if (dir_vnode != NULL){
			dbg(DBG_PRINT, "(GRADING2B)\n");
                        vput(dir_vnode);
                }
                if(file_vnode != NULL){
			dbg(DBG_PRINT, "(GRADING2B)\n");
                        vput(file_vnode);
                }
                return -EPERM;
        }

        KASSERT(NULL != dir_vnode->vn_ops->unlink);
	dbg(DBG_PRINT, "(GRADING2A 3.e)\n");
	dbg(DBG_PRINT, "(GRADING2B)\n");

        int return_state = dir_vnode->vn_ops->unlink(dir_vnode, name, namelen);
        // NOT_YET_IMPLEMENTED("VFS: do_unlink");
        vput(dir_vnode);
        vput(file_vnode);
        return return_state;
}

/* To link:
 *      o open_namev(from)
 *      o dir_namev(to)
 *      o call the destination dir's (to) link vn_ops.
 *      o return the result of link, or an error
 *
 * Remember to vput the vnodes returned from open_namev and dir_namev.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EEXIST
 *        to already exists.
 *      o ENOENT
 *        A directory component in from or to does not exist.
 *      o ENOTDIR
 *        A component used as a directory in from or to is not, in fact, a
 *        directory.
 *      o ENAMETOOLONG
 *        A component of from or to was too long.
 *      o EPERM
 *        from is a directory.
 */
int
do_link(const char *from, const char *to)
{
	
        if((strlen(from) > MAXPATHLEN) || (strlen(to) > MAXPATHLEN)){
	        dbg(DBG_PRINT, "link break 1\n");
                return -ENAMETOOLONG;
        }

        vnode_t * from_file_vnode = NULL;
        int error = open_namev(from, 0, &from_file_vnode, NULL);

        // check whether the path "from" is valid
        if(error < 0){
		dbg(DBG_PRINT, "(GRADING2B)\n");
                return error;
        }

        if(from_file_vnode->vn_mode == S_IFDIR){
                
               vput(from_file_vnode);
               dbg(DBG_PRINT, "link break 3\n");
               return -EPERM;
        }

        size_t to_namelen;
        const char * to_name; 
        vnode_t * to_dir_vnode = NULL;
        error = dir_namev(to, &to_namelen, &to_name, NULL, &to_dir_vnode);

        // check whether the path "to" is valid
        if(error < 0){
                vput(from_file_vnode);
		dbg(DBG_PRINT, "(GRADING2B)\n");
                return error;
        }//return 0;
	
        int return_state;
        vnode_t * to_file_vnode;
        error = lookup(to_dir_vnode, to_name, to_namelen, &to_file_vnode);
	dbg(DBG_PRINT, "link break 7\n");
        if(error == 0){
                vput(to_file_vnode);
                vput(from_file_vnode);
                vput(to_dir_vnode);
		dbg(DBG_PRINT, "link break 5\n");
                return -EEXIST;
        }
        // to file not exist
        else{
                return_state = to_dir_vnode->vn_ops->link(from_file_vnode, to_dir_vnode, to_name, to_namelen);
                vput(to_dir_vnode);
                vput(from_file_vnode);
		dbg(DBG_PRINT, "link break 6\n");
                return return_state;
        }
        


        //NOT_YET_IMPLEMENTED("VFS: do_link");
}

/*      o link newname to oldname
 *      o unlink oldname
 *      o return the value of unlink, or an error
 *
 * Note that this does not provide the same behavior as the
 * Linux system call (if unlink fails then two links to the
 * file could exist).
 */
int
do_rename(const char *oldname, const char *newname)
{
	
        int error = do_link(oldname, newname);
        if(error < 0){
		dbg(DBG_PRINT, "(GRADING2B)\n");
                return error;
        }int return_state = do_unlink(oldname);return return_state;
        //NOT_YET_IMPLEMENTED("VFS: do_rename");
}

/* Make the named directory the current process's cwd (current working
 * directory).  Don't forget to down the refcount to the old cwd (vput()) and
 * up the refcount to the new cwd (open_namev() or vget()). Return 0 on
 * success.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o ENOENT
 *        path does not exist.
 *      o ENAMETOOLONG
 *        A component of path was too long.
 *      o ENOTDIR
 *        A component of path is not a directory.
 */
int
do_chdir(const char *path)
{
	
        if(strlen(path) > MAXPATHLEN){
		dbg(DBG_PRINT, "chdir break 2\n");
               return -ENAMETOOLONG;
        }

        size_t namelen;
        char * name; 
        vnode_t * dir_vnode = NULL;
        int error = open_namev(path, 0, &dir_vnode, NULL);
        // int error = dir_namev(path, &namelen, &name, NULL, &dir_vnode);

        if(error < 0){
		dbg(DBG_PRINT, "(GRADING2B)\n");
                return error;
        }

        // path not a dir 
        if(dir_vnode->vn_mode != S_IFDIR){
                vput(dir_vnode);
		dbg(DBG_PRINT, "(GRADING2B)\n");
                return -ENOTDIR;
        }
        else{
                vnode_t * old_dir_vnode = curproc->p_cwd;
                curproc->p_cwd = dir_vnode;
                vput(old_dir_vnode);
		dbg(DBG_PRINT, "(GRADING2B)\n");
        }
	dbg(DBG_PRINT, "(GRADING2B)\n");
        return 0;

        // NOT_YET_IMPLEMENTED("VFS: do_chdir");
}

/* Call the readdir vn_op on the given fd, filling in the given dirent_t*.
 * If the readdir vn_op is successful, it will return a positive value which
 * is the number of bytes copied to the dirent_t.  You need to increment the
 * file_t's f_pos by this amount.  As always, be aware of refcounts, check
 * the return value of the fget and the virtual function, and be sure the
 * virtual function exists (is not null) before calling it.
 *
 * Return either 0 or sizeof(dirent_t), or -errno.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        Invalid file descriptor fd.
 *      o ENOTDIR
 *        File descriptor does not refer to a directory.
 */
int
do_getdent(int fd, struct dirent *dirp)
{
	if(fd < 0 || fd >= NFILES){
		dbg(DBG_PRINT, "(GRADING2B)\n");
		return -EBADF;
	}
        file_t * file = fget(fd);
        if(file == NULL){
		dbg(DBG_PRINT, "(GRADING2B)\n");
                return -EBADF;
        }

        if(file->f_vnode->vn_mode != S_IFDIR){
                fput(file);
		dbg(DBG_PRINT, "(GRADING2B)\n");
                return -ENOTDIR;
        }

        int offset = file->f_vnode->vn_ops->readdir(file->f_vnode, file->f_pos, dirp);
        fput(file);
        if(offset==0){
		dbg(DBG_PRINT, "(GRADING2B)\n");
                return 0;
        }
	file->f_pos += offset;
	dbg(DBG_PRINT, "(GRADING2B)\n");
        return sizeof(dirent_t);


        
        // NOT_YET_IMPLEMENTED("VFS: do_getdent");
}

/*
 * Modify f_pos according to offset and whence.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o EBADF
 *        fd is not an open file descriptor.
 *      o EINVAL
 *        whence is not one of SEEK_SET, SEEK_CUR, SEEK_END; or the resulting
 *        file offset would be negative.
 */
int
do_lseek(int fd, int offset, int whence)
{       
	if(fd < 0 || fd >= NFILES){
		dbg(DBG_PRINT, "(GRADING2B)\n");
		return -EBADF;
	}
        file_t * file = fget(fd);
        if(file == NULL){
		dbg(DBG_PRINT, "(GRADING2B)\n");
                return -EBADF;
        }

        off_t new_pos;

        if(whence != SEEK_CUR && whence != SEEK_END && whence != SEEK_SET){
                fput(file);
		dbg(DBG_PRINT, "(GRADING2B)\n");
                return -EINVAL;
        }
        else if(whence == SEEK_CUR){
                new_pos = file->f_pos + offset;
        }
        else if(whence == SEEK_END){
                new_pos = file->f_vnode->vn_len + offset;
        }
        else{
                new_pos = offset;
        }
        if(new_pos < 0){
                fput(file);
		dbg(DBG_PRINT, "(GRADING2B)\n");
                return -EINVAL;
        }
        file->f_pos = new_pos;
        fput(file);
        dbg(DBG_PRINT, "(GRADING2B)\n");
        return new_pos;
}

/*
 * Find the vnode associated with the path, and call the stat() vnode operation.
 *
 * Error cases you must handle for this function at the VFS level:
 *      o ENOENT
 *        A component of path does not exist.
 *      o ENOTDIR
 *        A component of the path prefix of path is not a directory.
 *      o ENAMETOOLONG
 *        A component of path was too long.
 *      o EINVAL
 *        path is an empty string.
 */
int
do_stat(const char *path, struct stat *buf)
{
	
        
	if(strlen(path) > MAXPATHLEN){
		dbg(DBG_PRINT, "stat break 1\n");
                return -ENAMETOOLONG;
        }
	

        if(strlen(path) == 0){
		dbg(DBG_PRINT, "(GRADING2B)\n");
                return -EINVAL;
        }

        vnode_t * dir_vnode = NULL;
        int error = open_namev(path, 0, &dir_vnode, NULL);

        if(error < 0){
		dbg(DBG_PRINT, "(GRADING2B)\n");
                return error;
        }
	
	KASSERT(NULL != dir_vnode->vn_ops->stat);
	dbg(DBG_PRINT, "(GRADING2A 3.f)\n");
	dbg(DBG_PRINT, "(GRADING2B)\n");
	
        int return_state = dir_vnode->vn_ops->stat(dir_vnode, buf);
        // NOT_YET_IMPLEMENTED("VFS: do_stat");
        vput(dir_vnode);
        return return_state;
}

#ifdef __MOUNTING__
/*
 * Implementing this function is not required and strongly discouraged unless
 * you are absolutely sure your Weenix is perfect.
 *
 * This is the syscall entry point into vfs for mounting. You will need to
 * create the fs_t struct and populate its fs_dev and fs_type fields before
 * calling vfs's mountfunc(). mountfunc() will use the fields you populated
 * in order to determine which underlying filesystem's mount function should
 * be run, then it will finish setting up the fs_t struct. At this point you
 * have a fully functioning file system, however it is not mounted on the
 * virtual file system, you will need to call vfs_mount to do this.
 *
 * There are lots of things which can go wrong here. Make sure you have good
 * error handling. Remember the fs_dev and fs_type buffers have limited size
 * so you should not write arbitrary length strings to them.
 */
int
do_mount(const char *source, const char *target, const char *type)
{
        NOT_YET_IMPLEMENTED("MOUNTING: do_mount");
        return -EINVAL;
}

/*
 * Implementing this function is not required and strongly discouraged unless
 * you are absolutley sure your Weenix is perfect.
 *
 * This function delegates all of the real work to vfs_umount. You should not worry
 * about freeing the fs_t struct here, that is done in vfs_umount. All this function
 * does is figure out which file system to pass to vfs_umount and do good error
 * checking.
 */
int
do_umount(const char *target)
{
        NOT_YET_IMPLEMENTED("MOUNTING: do_umount");
        return -EINVAL;
}
#endif
