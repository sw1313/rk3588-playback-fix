#include "../fixture.h"
#define MISC_DYNAMIC_MINOR 255
struct miscdevice {int minor;const char *name;const struct file_operations *fops;
    void *list[2],*parent,*this_device,*groups;const char *nodename;unsigned short mode;};
int misc_register(struct miscdevice *);
void misc_deregister(struct miscdevice *);
