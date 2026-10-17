// SPDX-License-Identifier: MIT
#define _GNU_SOURCE
#define VK_NO_PROTOTYPES
#include <hardware/hwvulkan.h>
#include <android/log.h>
#include <dlfcn.h>
#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdatomic.h>
#include <unistd.h>
#include <vulkan/vk_android_native_buffer.h>
#include "gralloc_bridge.h"

int eden_install_group_adapter(void*);
static pthread_once_t once = PTHREAD_ONCE_INIT;
static void* payload;
static const hwvulkan_module_t* real_module;
static PFN_vkGetInstanceProcAddr real_gipa;
static _Atomic(PFN_vkGetDeviceProcAddr) real_gdpa;
static _Atomic(PFN_vkCreateDevice) real_create_device;
static PFN_vkVoidFunction wrapped_gipa(VkInstance, const char*);
static PFN_vkVoidFunction wrapped_gdpa(VkDevice, const char*);

struct imported_image {
    VkDevice device;
    VkImage image;
    VkDeviceMemory memory;
    VkAllocationCallbacks allocator;
    int has_allocator;
    struct imported_image* next;
};
static pthread_mutex_t images_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct imported_image* images;
struct queue_device { VkQueue queue; VkDevice device; struct queue_device* next; };
static struct queue_device* queues;
static void remember_queue(VkDevice device, VkQueue queue) {
    if (!queue) return;
    pthread_mutex_lock(&images_mutex);
    for (struct queue_device* p = queues; p; p = p->next) {
        if (p->queue == queue) { pthread_mutex_unlock(&images_mutex); return; }
    }
    struct queue_device* p = malloc(sizeof(*p));
    if (p) { *p = (struct queue_device){queue, device, queues}; queues = p; }
    pthread_mutex_unlock(&images_mutex);
}
static void bridge_get_device_queue(VkDevice device, uint32_t family, uint32_t index,
                                    VkQueue* queue) {
    PFN_vkGetDeviceProcAddr gdpa = atomic_load(&real_gdpa);
    ((PFN_vkGetDeviceQueue)gdpa(device, "vkGetDeviceQueue"))(device, family, index, queue);
    remember_queue(device, *queue);
}
static void bridge_get_device_queue2(VkDevice device, const VkDeviceQueueInfo2* info,
                                     VkQueue* queue) {
    PFN_vkGetDeviceProcAddr gdpa = atomic_load(&real_gdpa);
    ((PFN_vkGetDeviceQueue2)gdpa(device, "vkGetDeviceQueue2"))(device, info, queue);
    remember_queue(device, *queue);
}
static VkResult bridge_release_image(VkQueue queue, uint32_t wait_count,
    const VkSemaphore* waits, VkImage image, int* native_fence) {
    (void)image;
    // This RC3/kbase build cannot export its Android release semaphore as a
    // sync_file. Consume the waits exactly once, then wait for a real GPU
    // fence before handing a completed buffer to Android with fence fd -1.
    // Never retry after the original export path: it may have consumed waits.
    *native_fence = -1;
    VkDevice device = VK_NULL_HANDLE;
    pthread_mutex_lock(&images_mutex);
    for (struct queue_device* p = queues; p; p = p->next)
        if (p->queue == queue) { device = p->device; break; }
    pthread_mutex_unlock(&images_mutex);
    if (!device) return VK_ERROR_INITIALIZATION_FAILED;
    PFN_vkGetDeviceProcAddr gdpa = atomic_load(&real_gdpa);
    PFN_vkCreateFence create = (PFN_vkCreateFence)gdpa(device, "vkCreateFence");
    PFN_vkDestroyFence destroy = (PFN_vkDestroyFence)gdpa(device, "vkDestroyFence");
    PFN_vkWaitForFences wait = (PFN_vkWaitForFences)gdpa(device, "vkWaitForFences");
    PFN_vkQueueSubmit submit = (PFN_vkQueueSubmit)gdpa(device, "vkQueueSubmit");
    if (!create || !destroy || !wait || !submit) return VK_ERROR_INITIALIZATION_FAILED;
    VkPipelineStageFlags* stages = wait_count ? calloc(wait_count, sizeof(*stages)) : NULL;
    if (wait_count && !stages) return VK_ERROR_OUT_OF_HOST_MEMORY;
    for (uint32_t i = 0; i < wait_count; ++i) stages[i] = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    VkFenceCreateInfo info = {.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    VkFence fence;
    VkResult result = create(device, &info, NULL, &fence);
    if (result == VK_SUCCESS) {
        VkSubmitInfo empty = {.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            .waitSemaphoreCount = wait_count, .pWaitSemaphores = waits, .pWaitDstStageMask = stages};
        result = submit(queue, 1, &empty, fence);
        if (result == VK_SUCCESS) result = wait(device, 1, &fence, VK_TRUE, UINT64_MAX);
        destroy(device, fence, NULL);
    }
    free(stages);
    if (result != VK_SUCCESS)
        __android_log_print(ANDROID_LOG_ERROR, "EdenRC3", "GPU release fence failed: %d", result);
    return result;
}

static VkResult bridge_create_device(VkPhysicalDevice physical,
    const VkDeviceCreateInfo* info, const VkAllocationCallbacks* allocator, VkDevice* device) {
    if (info->enabledExtensionCount > 1024) return VK_ERROR_INITIALIZATION_FAILED;
    const char** names = calloc(info->enabledExtensionCount + 3, sizeof(*names));
    if (!names) return VK_ERROR_OUT_OF_HOST_MEMORY;
    uint32_t count = info->enabledExtensionCount;
    if (count) memcpy(names, info->ppEnabledExtensionNames, count * sizeof(*names));
    const char* required[] = {"VK_KHR_external_memory_fd",
        "VK_EXT_external_memory_dma_buf", "VK_EXT_image_drm_format_modifier"};
    for (unsigned i = 0; i < 3; ++i) {
        int found = 0;
        for (uint32_t j = 0; j < count; ++j) if (!strcmp(names[j], required[i])) found = 1;
        if (!found) names[count++] = required[i];
    }
    VkDeviceCreateInfo local = *info;
    local.enabledExtensionCount = count; local.ppEnabledExtensionNames = names;
    PFN_vkCreateDevice create = atomic_load(&real_create_device);
    VkResult result = create ? create(physical, &local, allocator, device) : VK_ERROR_INITIALIZATION_FAILED;
    free(names);
    return result;
}

static VkResult bridge_create_image(VkDevice device, const VkImageCreateInfo* info,
    const VkAllocationCallbacks* allocator, VkImage* image) {
    PFN_vkGetDeviceProcAddr gdpa = atomic_load(&real_gdpa);
    PFN_vkCreateImage create = (PFN_vkCreateImage)gdpa(device, "vkCreateImage");
    const VkBaseInStructure* ext = info->pNext;
    while (ext && ext->sType != VK_STRUCTURE_TYPE_NATIVE_BUFFER_ANDROID) ext = ext->pNext;
    if (!ext) return create(device, info, allocator, image);
    // Android 12 supplies the spec-8 prefix only: never read usage3 or ahb.
    const VkNativeBufferANDROID* anb = (const void*)ext;
    __android_log_print(ANDROID_LOG_INFO, "EdenRC3", "ANB %ux%u fmt=%d stride=%d fds=%d",
        info->extent.width, info->extent.height, anb->format, anb->stride,
        anb->handle ? anb->handle->numFds : -1);
    if ((const void*)ext != info->pNext || info->flags & VK_IMAGE_CREATE_PROTECTED_BIT)
        return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    struct eden_gralloc_layout layout;
    int query = eden_query_gralloc(anb->handle, &layout);
    if (query) {
        __android_log_print(ANDROID_LOG_ERROR, "EdenRC3", "ROM metadata rejected: %d", query);
        return VK_ERROR_INVALID_EXTERNAL_HANDLE;
    }
    uint32_t expected = 0;
    switch (info->format) {
        case VK_FORMAT_R8G8B8A8_UNORM: case VK_FORMAT_R8G8B8A8_SRGB: expected = 0x34324241; break;
        case VK_FORMAT_B8G8R8A8_UNORM: case VK_FORMAT_B8G8R8A8_SRGB: expected = 0x34325241; break;
        case VK_FORMAT_A2B10G10R10_UNORM_PACK32: expected = 0x30334241; break;
        default: return VK_ERROR_FORMAT_NOT_SUPPORTED;
    }
    if (layout.fourcc != expected) return VK_ERROR_FORMAT_NOT_SUPPORTED;
    struct imported_image* owned = calloc(1, sizeof(*owned));
    if (!owned) return VK_ERROR_OUT_OF_HOST_MEMORY;
    VkSubresourceLayout plane = {.offset = layout.offset, .rowPitch = layout.pitch};
    VkImageDrmFormatModifierExplicitCreateInfoEXT modifier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_DRM_FORMAT_MODIFIER_EXPLICIT_CREATE_INFO_EXT,
        .pNext = anb->pNext, .drmFormatModifier = layout.modifier,
        .drmFormatModifierPlaneCount = 1, .pPlaneLayouts = &plane,
    };
    VkExternalMemoryImageCreateInfo external = {
        .sType = VK_STRUCTURE_TYPE_EXTERNAL_MEMORY_IMAGE_CREATE_INFO, .pNext = &modifier,
        .handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT,
    };
    VkImageCreateInfo local = *info;
    local.pNext = &external;
    local.tiling = VK_IMAGE_TILING_DRM_FORMAT_MODIFIER_EXT;
    VkResult result = create(device, &local, allocator, image);
    if (result != VK_SUCCESS) { free(owned); return result; }
    PFN_vkDestroyImage destroy = (PFN_vkDestroyImage)gdpa(device, "vkDestroyImage");
    PFN_vkGetImageMemoryRequirements requirements =
        (PFN_vkGetImageMemoryRequirements)gdpa(device, "vkGetImageMemoryRequirements");
    PFN_vkGetMemoryFdPropertiesKHR fd_properties =
        (PFN_vkGetMemoryFdPropertiesKHR)gdpa(device, "vkGetMemoryFdPropertiesKHR");
    PFN_vkAllocateMemory allocate = (PFN_vkAllocateMemory)gdpa(device, "vkAllocateMemory");
    PFN_vkBindImageMemory bind = (PFN_vkBindImageMemory)gdpa(device, "vkBindImageMemory");
    PFN_vkFreeMemory release = (PFN_vkFreeMemory)gdpa(device, "vkFreeMemory");
    if (!destroy || !requirements || !fd_properties || !allocate || !bind || !release) {
        if (destroy) destroy(device, *image, allocator);
        free(owned); *image = VK_NULL_HANDLE;
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
    VkMemoryRequirements req;
    requirements(device, *image, &req);
    VkMemoryFdPropertiesKHR props = {.sType = VK_STRUCTURE_TYPE_MEMORY_FD_PROPERTIES_KHR};
    result = fd_properties(device, VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, layout.fd, &props);
    uint32_t types = props.memoryTypeBits & req.memoryTypeBits;
    if (result == VK_SUCCESS && (!types || req.size > layout.allocation))
        result = VK_ERROR_INVALID_EXTERNAL_HANDLE;
    int fd = -1;
    if (result == VK_SUCCESS && (fd = dup(layout.fd)) < 0) result = VK_ERROR_OUT_OF_HOST_MEMORY;
    if (result == VK_SUCCESS) {
        VkMemoryDedicatedAllocateInfo dedicated = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_DEDICATED_ALLOCATE_INFO, .image = *image};
        VkImportMemoryFdInfoKHR import = {
            .sType = VK_STRUCTURE_TYPE_IMPORT_MEMORY_FD_INFO_KHR, .pNext = &dedicated,
            .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_DMA_BUF_BIT_EXT, .fd = fd};
        VkMemoryAllocateInfo alloc = {.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
            .pNext = &import, .allocationSize = req.size,
            .memoryTypeIndex = (uint32_t)__builtin_ctz(types)};
        result = allocate(device, &alloc, allocator, &owned->memory);
        if (result == VK_SUCCESS) fd = -1; // Ownership transfers only on success.
    }
    if (fd >= 0) close(fd);
    if (result == VK_SUCCESS) result = bind(device, *image, owned->memory, 0);
    if (result != VK_SUCCESS) {
        destroy(device, *image, allocator);
        if (owned->memory) release(device, owned->memory, allocator);
        *image = VK_NULL_HANDLE;
        free(owned);
        __android_log_print(ANDROID_LOG_ERROR, "EdenRC3", "ANB DMA import failed %d", result);
        return result;
    }
    owned->device = device; owned->image = *image;
    if (allocator) { owned->allocator = *allocator; owned->has_allocator = 1; }
    pthread_mutex_lock(&images_mutex);
    owned->next = images; images = owned;
    pthread_mutex_unlock(&images_mutex);
    __android_log_print(ANDROID_LOG_INFO, "EdenRC3", "ANB DMA import bound successfully");
    return VK_SUCCESS;
}

static void bridge_destroy_image(VkDevice device, VkImage image,
                                const VkAllocationCallbacks* allocator) {
    PFN_vkGetDeviceProcAddr gdpa = atomic_load(&real_gdpa);
    struct imported_image* owned = NULL;
    pthread_mutex_lock(&images_mutex);
    for (struct imported_image** p = &images; *p; p = &(*p)->next) {
        if ((*p)->device == device && (*p)->image == image) {
            owned = *p; *p = owned->next; break;
        }
    }
    pthread_mutex_unlock(&images_mutex);
    ((PFN_vkDestroyImage)gdpa(device, "vkDestroyImage"))(device, image, allocator);
    if (owned) {
        ((PFN_vkFreeMemory)gdpa(device, "vkFreeMemory"))(device, owned->memory,
            owned->has_allocator ? &owned->allocator : NULL);
        free(owned);
    }
}

static void bridge_destroy_device(VkDevice device, const VkAllocationCallbacks* allocator) {
    // Also release our private imports if an application destroys a device
    // without separately destroying every window image after an error.
    for (;;) {
        pthread_mutex_lock(&images_mutex);
        struct imported_image* found = images;
        while (found && found->device != device) found = found->next;
        VkImage image = found ? found->image : VK_NULL_HANDLE;
        VkAllocationCallbacks copy;
        int has_allocator = found && found->has_allocator;
        if (has_allocator) copy = found->allocator;
        pthread_mutex_unlock(&images_mutex);
        if (!image) break;
        bridge_destroy_image(device, image, has_allocator ? &copy : NULL);
    }
    PFN_vkGetDeviceProcAddr gdpa = atomic_load(&real_gdpa);
    pthread_mutex_lock(&images_mutex);
    struct queue_device** entry = &queues;
    while (*entry) {
        if ((*entry)->device == device) {
            struct queue_device* old = *entry; *entry = old->next; free(old);
        } else entry = &(*entry)->next;
    }
    pthread_mutex_unlock(&images_mutex);
    ((PFN_vkDestroyDevice)gdpa(device, "vkDestroyDevice"))(device, allocator);
}

static PFN_vkVoidFunction wrapped_gdpa(VkDevice device, const char* name) {
    PFN_vkGetDeviceProcAddr gdpa = atomic_load(&real_gdpa);
    PFN_vkVoidFunction p = gdpa ? gdpa(device, name) : NULL;
    if (!p) return NULL;
    if (!strcmp(name, "vkCreateImage")) return (PFN_vkVoidFunction)bridge_create_image;
    if (!strcmp(name, "vkDestroyImage")) return (PFN_vkVoidFunction)bridge_destroy_image;
    if (!strcmp(name, "vkDestroyDevice")) return (PFN_vkVoidFunction)bridge_destroy_device;
    if (!strcmp(name, "vkGetDeviceQueue")) return (PFN_vkVoidFunction)bridge_get_device_queue;
    if (!strcmp(name, "vkGetDeviceQueue2")) return (PFN_vkVoidFunction)bridge_get_device_queue2;
    if (!strcmp(name, "vkQueueSignalReleaseImageANDROID")) return (PFN_vkVoidFunction)bridge_release_image;
    return p;
}
static PFN_vkVoidFunction wrapped_gipa(VkInstance instance, const char* name) {
    PFN_vkVoidFunction p = real_gipa ? real_gipa(instance, name) : NULL;
    if (!p) return NULL;
    if (instance) atomic_store(&real_gdpa,
        (PFN_vkGetDeviceProcAddr)real_gipa(instance, "vkGetDeviceProcAddr"));
    if (!strcmp(name, "vkGetInstanceProcAddr")) return (PFN_vkVoidFunction)wrapped_gipa;
    if (!strcmp(name, "vkGetDeviceProcAddr")) return (PFN_vkVoidFunction)wrapped_gdpa;
    if (!strcmp(name, "vkCreateDevice")) {
        atomic_store(&real_create_device, (PFN_vkCreateDevice)p);
        return (PFN_vkVoidFunction)bridge_create_device;
    }
    if (!strcmp(name, "vkCreateImage")) return (PFN_vkVoidFunction)bridge_create_image;
    if (!strcmp(name, "vkDestroyImage")) return (PFN_vkVoidFunction)bridge_destroy_image;
    if (!strcmp(name, "vkDestroyDevice")) return (PFN_vkVoidFunction)bridge_destroy_device;
    if (!strcmp(name, "vkGetDeviceQueue")) return (PFN_vkVoidFunction)bridge_get_device_queue;
    if (!strcmp(name, "vkGetDeviceQueue2")) return (PFN_vkVoidFunction)bridge_get_device_queue2;
    if (!strcmp(name, "vkQueueSignalReleaseImageANDROID")) return (PFN_vkVoidFunction)bridge_release_image;
    return p;
}

static void initialize_payload(void) {
    // Preserve exact BC decoding rather than the upstream lossy AFRC default.
    if (setenv("PANVK_BC_AFRC", "0", 1) != 0) return;
    Dl_info info = {0};
    if (!dladdr((void*)initialize_payload, &info) || !info.dli_fname) return;
    const char* slash = strrchr(info.dli_fname, '/');
    if (!slash) return;
    char path[PATH_MAX];
    int count = snprintf(path, sizeof(path), "%.*s/panvk_rc3_payload.so",
                         (int)(slash - info.dli_fname), info.dli_fname);
    if (count <= 0 || (size_t)count >= sizeof(path)) return;
    payload = dlopen(path, RTLD_NOW | RTLD_LOCAL);
    if (!payload) {
        __android_log_print(ANDROID_LOG_ERROR, "EdenRC3", "Payload load failed: %s", dlerror());
        return;
    }
    int group_status=eden_install_group_adapter(payload);
    if(group_status){__android_log_print(ANDROID_LOG_ERROR,"EdenRC3","Group adapter rejected payload: %d",group_status);return;}
    real_module = dlsym(payload, HAL_MODULE_INFO_SYM_AS_STR);
    real_gipa = (PFN_vkGetInstanceProcAddr)dlsym(payload, "vk_icdGetInstanceProcAddr");
    if (!real_module || !real_module->common.methods || !real_gipa) {
        real_module = NULL;
        real_gipa = NULL;
        __android_log_print(ANDROID_LOG_ERROR, "EdenRC3", "Missing payload HAL or ICD interface");
        return;
    }
    __android_log_print(ANDROID_LOG_INFO, "EdenRC3",
                        "RC3 Test8: G610 address64 fix; single-CSG scheduling diagnostic; exact BC");
}

struct wrapped_hal { hwvulkan_device_t hal; struct hw_device_t* original; };
static int close_driver(struct hw_device_t* device) {
    struct wrapped_hal* wrapped = (void*)device;
    int result = wrapped->original->close(wrapped->original);
    free(wrapped);
    return result;
}
static int open_driver(const struct hw_module_t* module, const char* id,
                       struct hw_device_t** device) {
    (void)module;
    pthread_once(&once, initialize_payload);
    if (!real_module) return -ENODEV;
    struct wrapped_hal* wrapped = calloc(1, sizeof(*wrapped));
    if (!wrapped) return -ENOMEM;
    int result = real_module->common.methods->open(&real_module->common, id, &wrapped->original);
    if (result) { free(wrapped); return result; }
    wrapped->hal = *(hwvulkan_device_t*)wrapped->original;
    wrapped->hal.common.close = close_driver;
    wrapped->hal.GetInstanceProcAddr = wrapped_gipa;
    *device = &wrapped->hal.common;
    return 0;
}
static struct hw_module_methods_t methods = {.open = open_driver};

__attribute__((visibility("default"))) hwvulkan_module_t HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .module_api_version = HWVULKAN_MODULE_API_VERSION_0_1,
        .hal_api_version = HARDWARE_HAL_API_VERSION,
        .id = HWVULKAN_HARDWARE_MODULE_ID,
        .name = "PanVK RC3 RK3588 Android 12 validation loader",
        .author = "Mesa contributors; local compatibility loader",
        .methods = &methods,
    },
};

__attribute__((visibility("default")))
PFN_vkVoidFunction vk_icdGetInstanceProcAddr(VkInstance instance, const char* name) {
    pthread_once(&once, initialize_payload);
    return wrapped_gipa(instance, name);
}

__attribute__((visibility("default")))
VkResult vk_icdNegotiateLoaderICDInterfaceVersion(uint32_t* version) {
    pthread_once(&once, initialize_payload);
    if (!payload || !real_gipa) return VK_ERROR_INCOMPATIBLE_DRIVER;
    VkResult (*negotiate)(uint32_t*) = dlsym(payload, "vk_icdNegotiateLoaderICDInterfaceVersion");
    return negotiate ? negotiate(version) : VK_ERROR_INCOMPATIBLE_DRIVER;
}

__attribute__((visibility("default")))
PFN_vkVoidFunction vk_icdGetPhysicalDeviceProcAddr(VkInstance instance, const char* name) {
    pthread_once(&once, initialize_payload);
    PFN_vkVoidFunction (*get)(VkInstance, const char*) =
        payload ? dlsym(payload, "vk_icdGetPhysicalDeviceProcAddr") : NULL;
    return get ? get(instance, name) : NULL;
}
