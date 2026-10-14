#include <vulkan/vulkan.h>
#include <stdio.h>
#include <stdlib.h>
#include <dlfcn.h>
int main(void) {
    void *library=dlopen("libvulkan.so.1",RTLD_NOW|RTLD_LOCAL);
    if (!library) { fputs(dlerror(),stderr); return 1; }
    PFN_vkCreateInstance create=(PFN_vkCreateInstance)dlsym(library,"vkCreateInstance");
    PFN_vkEnumeratePhysicalDevices enumerate=(PFN_vkEnumeratePhysicalDevices)dlsym(library,"vkEnumeratePhysicalDevices");
    PFN_vkGetPhysicalDeviceProperties properties=(PFN_vkGetPhysicalDeviceProperties)dlsym(library,"vkGetPhysicalDeviceProperties");
    PFN_vkDestroyInstance destroy=(PFN_vkDestroyInstance)dlsym(library,"vkDestroyInstance");
    if (!create||!enumerate||!properties||!destroy) return 2;
    VkInstanceCreateInfo info={.sType=VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};VkInstance instance=VK_NULL_HANDLE;
    VkResult result=create(&info,NULL,&instance);printf("CreateInstance=%d\n",result);if(result!=VK_SUCCESS)return 3;
    uint32_t count=0;result=enumerate(instance,&count,NULL);printf("Enumerate=%d devices=%u\n",result,count);
    VkPhysicalDevice *devices=calloc(count?count:1,sizeof(*devices));
    if(result==VK_SUCCESS && devices && count) {
        result=enumerate(instance,&count,devices);
        for(uint32_t i=0;result==VK_SUCCESS&&i<count;++i) {VkPhysicalDeviceProperties p;properties(devices[i],&p);printf("%s api=%u.%u.%u type=%u\n",p.deviceName,VK_VERSION_MAJOR(p.apiVersion),VK_VERSION_MINOR(p.apiVersion),VK_VERSION_PATCH(p.apiVersion),p.deviceType);}
    }
    int ok=result==VK_SUCCESS && devices && count>0;
    free(devices);destroy(instance,NULL);dlclose(library);return ok?0:4;
}
