/* fhandler_dev.cc, Implement /dev.

   Copyright 2012, 2013 Red Hat, Inc.

This file is part of Cygwin.

This software is a copyrighted work licensed under the terms of the
Cygwin license.  Please consult the file "CYGWIN_LICENSE" for
details. */

#include "winsup.h"
#include <stdlib.h>
#include <sys/statvfs.h>
#include "path.h"
#include "fhandler.h"
#include "dtable.h"
#include "cygheap.h"
#include "devices.h"

#define _COMPILING_NEWLIB
#include <dirent.h>

#define dev_prefix_len (sizeof ("/dev"))
#define dev_storage_scan_start (dev_storage + 1)
#define dev_storage_size (dev_storage_end - dev_storage_scan_start)

static int
device_cmp (const void *a, const void *b)
{
  return strcmp (((const device *) a)->name,
		 ((const device *) b)->name + dev_prefix_len);
}

fhandler_dev::fhandler_dev () :
  fhandler_disk_file (), devidx (NULL), dir_exists (true)
{
}

int
fhandler_dev::open (int flags, mode_t mode)
{
  if ((flags & (O_CREAT | O_EXCL)) == (O_CREAT | O_EXCL))
    {
      set_errno (EEXIST);
      return 0;
    }
  if (flags & O_WRONLY)
    {
      set_errno (EISDIR);
      return 0;
    }
  int ret = fhandler_disk_file::open (flags, mode);
  if (!ret)
    {
      flags |= O_DIROPEN;
      set_flags (flags);
      nohandle (true);
    }
  return 1;
}

int
fhandler_dev::close ()
{
  if (nohandle ())
    return 0;
  return fhandler_disk_file::close ();
}

int __reg2
fhandler_dev::fstat (struct __stat64 *st)
{
  /* If /dev really exists on disk, return correct disk information. */
  if (pc.fs_got_fs ())
    return fhandler_disk_file::fstat (st);
  /* Otherwise fake virtual filesystem. */
  fhandler_base::fstat (st);
  st->st_ino = 2;
  st->st_mode = S_IFDIR | STD_RBITS | STD_XBITS;
  st->st_nlink = 1;
  return 0;
}

int __stdcall
fhandler_dev::fstatvfs (struct statvfs *sfs)
{
  int ret = -1, opened = 0;
  HANDLE fh = get_handle ();

  if (!fh && !nohandle ())
    {
      if (!open (O_RDONLY, 0))
	return -1;
      opened = 1;
    }
  if (!nohandle ())
    {
      ret = fhandler_disk_file::fstatvfs (sfs);
      goto out;
    }
  /* Virtual file system.  Just return an empty buffer with a few values
     set to something useful.  Just as on Linux. */
  memset (sfs, 0, sizeof (*sfs));
  sfs->f_bsize = sfs->f_frsize = 4096;
  sfs->f_namemax = NAME_MAX;
  ret = 0;

out:
  if (opened)
    close ();
  return ret;
}

DIR *
fhandler_dev::opendir (int fd)
{
  DIR *dir;
  DIR *res = NULL;

  dir = fhandler_disk_file::opendir (fd);
  if (dir)
    return dir;
  if ((dir = (DIR *) malloc (sizeof (DIR))) == NULL)
    set_errno (ENOMEM);
  else if ((dir->__d_dirent =
	    (struct dirent *) malloc (sizeof (struct dirent))) == NULL)
    {
      set_errno (ENOMEM);
      goto free_dir;
    }
  else
    {
      cygheap_fdnew cfd;
      if (cfd < 0 && fd < 0)
	goto free_dirent;

      dir->__d_dirname = NULL;
      dir->__d_dirent->__d_version = __DIRENT_VERSION;
      dir->__d_cookie = __DIRENT_COOKIE;
      dir->__handle = INVALID_HANDLE_VALUE;
      dir->__d_position = 0;
      dir->__flags = 0;
      dir->__d_internal = 0;

      if (fd >= 0)
	dir->__d_fd = fd;
      else
	{
	  cfd = this;
	  dir->__d_fd = cfd;
	  cfd->nohandle (true);
	}
      set_close_on_exec (true);
      dir->__fh = this;
      devidx = dev_storage_scan_start;
      res = dir;
    }

  syscall_printf ("%p = opendir (%s)", res, get_name ());
  return res;

free_dirent:
  free (dir->__d_dirent);
free_dir:
  free (dir);
  return res;
}

int
fhandler_dev::readdir (DIR *dir, dirent *de)
{
  int ret;
  const device *curdev;
  device dev;

  if (!devidx)
    {
      while ((ret = fhandler_disk_file::readdir (dir, de)) == 0)
	{
	  /* Avoid to print devices for which users have created files under
	     /dev already, for instance by using the old script from Igor
	     Peshansky. */
	  dev.name = de->d_name;
	  if (!bsearch (&dev, dev_storage_scan_start, dev_storage_size,
			sizeof dev, device_cmp))
	    break;
	}
      if (ret != ENMFILE)
	goto out;
      devidx = dev_storage_scan_start;
    }

  /* Now start processing our internal dev table. */
  ret = ENMFILE;
  while ((curdev = devidx++) < dev_storage_end)
    {
      /* If exists returns < 0 it means that the device can be used by a
	 program but its use is deprecated and, so, it is not returned
	 by readdir(().  */
      if (curdev->exists () <= 0)
	continue;
      ++dir->__d_position;
      strcpy (de->d_name, curdev->name + dev_prefix_len);
      if (curdev->get_major () == DEV_TTY_MAJOR
	  && (curdev->is_device (FH_CONIN)
	      || curdev->is_device (FH_CONOUT)
	      || curdev->is_device (FH_CONSOLE)))
	{
	  /* Make sure conin, conout, and console have the same inode number
	     as the current consX. */
	  de->d_ino = myself->ctty;
	}
      else
	de->d_ino = curdev->get_device ();
      de->d_type = curdev->type ();
      ret = 0;
      break;
    }

out:
  debug_printf ("returning %d", ret);
  return ret;
}

void
fhandler_dev::rewinddir (DIR *dir)
{
  devidx = dir_exists ? NULL : dev_storage_scan_start;
  fhandler_disk_file::rewinddir (dir);
}
