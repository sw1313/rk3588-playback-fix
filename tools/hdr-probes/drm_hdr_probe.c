/* Read-only DRM property inventory. No display property is changed. */
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "drm.h"
#include "drm_mode.h"

static void *array(uint32_t n, size_t size) {
    if (n > 4096) { fprintf(stderr, "oversize array\n"); exit(2); }
    void *p = calloc(n ? n : 1, size);
    if (!p) exit(2);
    return p;
}
static void formats(int fd, uint32_t id) {
    struct drm_mode_get_plane p={.plane_id=id};
    if (ioctl(fd,DRM_IOCTL_MODE_GETPLANE,&p)) return;
    uint32_t capacity=p.count_format_types;
    uint32_t *f=array(capacity,sizeof(*f));p.format_type_ptr=(uintptr_t)f;
    if (!ioctl(fd,DRM_IOCTL_MODE_GETPLANE,&p)) {
        printf("PLANE_FORMATS %u possible_crtcs=0x%x",id,p.possible_crtcs);
        for (uint32_t i=0;i<p.count_format_types && i<capacity;i++) {
            char name[5]={f[i]&255,(f[i]>>8)&255,(f[i]>>16)&255,(f[i]>>24)&255,0};
            printf(" %s(0x%08x)",name,f[i]);
        }
        printf("\n");
    }
    free(f);
}
static void properties(int fd, uint32_t id, uint32_t type, const char *kind) {
    struct drm_mode_obj_get_properties o = { .obj_id=id, .obj_type=type };
    if (ioctl(fd, DRM_IOCTL_MODE_OBJ_GETPROPERTIES, &o)) {
        printf("ERROR %s %u %s\n", kind, id, strerror(errno)); return;
    }
    uint32_t capacity=o.count_props;
    uint32_t *ids=array(capacity, sizeof(*ids));
    uint64_t *values=array(capacity, sizeof(*values));
    o.props_ptr=(uintptr_t)ids; o.prop_values_ptr=(uintptr_t)values;
    if (!ioctl(fd, DRM_IOCTL_MODE_OBJ_GETPROPERTIES, &o)) {
        printf("OBJECT %s %u\n", kind, id);
        for (uint32_t i=0; i<o.count_props && i<capacity; i++) {
            struct drm_mode_get_property p={.prop_id=ids[i]};
            if (ioctl(fd, DRM_IOCTL_MODE_GETPROPERTY, &p)) continue;
            p.name[sizeof(p.name)-1]=0;
            printf("PROP %u %s flags=0x%x value=%" PRIu64 "\n",ids[i],p.name,p.flags,values[i]);
            if (!strcmp(p.name,"EOTF") && (p.flags & DRM_MODE_PROP_RANGE) && p.count_values==2) {
                uint64_t range[2]={0};p.values_ptr=(uintptr_t)range;
                if (!ioctl(fd,DRM_IOCTL_MODE_GETPROPERTY,&p))
                    printf("RANGE EOTF min=%" PRIu64 " max=%" PRIu64 "\n",range[0],range[1]);
            }
            if (!strcmp(p.name,"FEATURE") && p.count_enum_blobs) {
                uint32_t enums=p.count_enum_blobs;
                struct drm_mode_property_enum *e=array(enums,sizeof(*e));
                uint64_t *ranges=array(p.count_values,sizeof(*ranges));
                p.enum_blob_ptr=(uintptr_t)e;p.values_ptr=(uintptr_t)ranges;
                if (!ioctl(fd,DRM_IOCTL_MODE_GETPROPERTY,&p)) {
                    for (uint32_t j=0;j<p.count_enum_blobs && j<enums;j++) {
                        e[j].name[sizeof(e[j].name)-1]=0;
                        printf("FEATURE %s bit=%" PRIu64 " enabled=%d\n",e[j].name,(uint64_t)e[j].value,
                            e[j].value<64 ? !!(values[i] & (UINT64_C(1)<<e[j].value)) : 0);
                    }
                }
                free(e);free(ranges);
            }
            if ((p.flags & DRM_MODE_PROP_BLOB) && values[i] &&
                (strstr(p.name,"HDR") || strstr(p.name,"DOVI") || strstr(p.name,"VSIF"))) {
                struct drm_mode_get_blob b={.blob_id=(uint32_t)values[i]};
                if (!ioctl(fd,DRM_IOCTL_MODE_GETPROPBLOB,&b) && b.length<=65536) {
                    uint32_t bytes=b.length;
                    uint8_t *data=calloc(bytes ? bytes : 1,1); if (!data) exit(2);
                    b.data=(uintptr_t)data;
                    if (!ioctl(fd,DRM_IOCTL_MODE_GETPROPBLOB,&b)) {
                        printf("BLOB %s bytes=%u hex=",p.name,bytes);
                        for (uint32_t j=0;j<bytes && j<256;j++) printf("%02x",data[j]);
                        printf("\n");
                    }
                    free(data);
                }
            }
        }
    }
    free(ids); free(values);
}
int main(void) {
    int fd=open("/dev/dri/card0",O_RDWR|O_CLOEXEC);
    if (fd<0) { perror("open"); return 1; }
    /* These flags only enable property visibility on this file descriptor. */
    struct drm_set_client_cap cap={.capability=DRM_CLIENT_CAP_ATOMIC,.value=1};
    if (ioctl(fd,DRM_IOCTL_SET_CLIENT_CAP,&cap)) perror("atomic visibility");
    struct drm_mode_card_res r={0};
    if (ioctl(fd,DRM_IOCTL_MODE_GETRESOURCES,&r)) { perror("resources"); close(fd); return 1; }
    uint32_t nc=r.count_connectors, nr=r.count_crtcs;
    uint32_t *connectors=array(nc,sizeof(uint32_t)),*crtcs=array(nr,sizeof(uint32_t));
    uint32_t *fbs=array(r.count_fbs,sizeof(uint32_t)),*encoders=array(r.count_encoders,sizeof(uint32_t));
    r.fb_id_ptr=(uintptr_t)fbs;r.encoder_id_ptr=(uintptr_t)encoders;
    r.connector_id_ptr=(uintptr_t)connectors;r.crtc_id_ptr=(uintptr_t)crtcs;
    if (ioctl(fd,DRM_IOCTL_MODE_GETRESOURCES,&r)) { perror("resources data"); return 1; }
    for (uint32_t i=0;i<r.count_connectors && i<nc;i++) properties(fd,connectors[i],DRM_MODE_OBJECT_CONNECTOR,"connector");
    for (uint32_t i=0;i<r.count_crtcs && i<nr;i++) properties(fd,crtcs[i],DRM_MODE_OBJECT_CRTC,"crtc");
    struct drm_mode_get_plane_res planes={0};
    if (!ioctl(fd,DRM_IOCTL_MODE_GETPLANERESOURCES,&planes)) {
        uint32_t np=planes.count_planes;
        uint32_t *ids=array(np,sizeof(uint32_t)); planes.plane_id_ptr=(uintptr_t)ids;
        if (!ioctl(fd,DRM_IOCTL_MODE_GETPLANERESOURCES,&planes))
            for (uint32_t i=0;i<planes.count_planes && i<np;i++) {
                formats(fd,ids[i]);
                properties(fd,ids[i],DRM_MODE_OBJECT_PLANE,"plane");
            }
        free(ids);
    }
    free(connectors);free(crtcs);free(fbs);free(encoders);close(fd);return 0;
}
