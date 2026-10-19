/* Only dispatcher types, never used as evidence of native private ABI. */
struct file {int unused;};
struct file_operations {
    void *owner;
    void *unused[9];
    long (*unlocked_ioctl)(struct file *,unsigned int,unsigned long);
    long (*compat_ioctl)(struct file *,unsigned int,unsigned long);
    void *mmap,*mmap_flags,*open;
};
