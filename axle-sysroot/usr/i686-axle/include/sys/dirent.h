
struct dirent {

	unsigned long  d_ino;     /* Inode number */
	unsigned long  d_off;     /* Offset to next linux_dirent */
	unsigned short d_reclen;  /* Length of this linux_dirent */
	char           d_name[];  /* Filename (null-terminated) */
	/* length is actually (d_reclen - 2 -
	   offsetof(struct linux_dirent, d_name)) */
	/*
	   char           pad;       // Zero padding byte
	   char           d_type;    // File type (only since Linux
	// 2.6.4); offset is (d_reclen - 1)
	*/
};
