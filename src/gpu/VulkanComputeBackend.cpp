#include "gpu/VulkanComputeBackend.h"
#include "gpu/GpuFrameFilter.h"
#include "core/Logger.h"

#ifdef VIEWCAM_HAVE_VULKAN
#include "gpu/ProofComputeSpv.h"
#include <vulkan/vulkan.h>
#include <cstring>
#include <vector>

namespace {
const char *vendorName(uint32_t id) {
    switch (id) {
    case 0x10DE: return "NVIDIA";
    case 0x1002: return "AMD";
    case 0x8086: return "Intel";
    case 0x13B5: return "ARM";
    case 0x5143: return "Qualcomm";
    default:     return "Unknown vendor";
    }
}
} // namespace

// All Vulkan handles live here so the header stays Vulkan-free.
struct VulkanComputeBackend::Impl {
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice phys = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue queue = VK_NULL_HANDLE;
    uint32_t queueFamily = 0;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
    VkDescriptorPool pool = VK_NULL_HANDLE;
    VkDescriptorSet dset = VK_NULL_HANDLE;
    VkShaderModule shader = VK_NULL_HANDLE;
    VkPipelineLayout pipeLayout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkCommandPool cmdPool = VK_NULL_HANDLE;

    static constexpr uint32_t kCount = 1024;
    static constexpr VkDeviceSize kBytes = kCount * sizeof(float);

    void destroy() {
        if (device) {
            if (pipeline) vkDestroyPipeline(device, pipeline, nullptr);
            if (pipeLayout) vkDestroyPipelineLayout(device, pipeLayout, nullptr);
            if (shader) vkDestroyShaderModule(device, shader, nullptr);
            if (pool) vkDestroyDescriptorPool(device, pool, nullptr);
            if (dsl) vkDestroyDescriptorSetLayout(device, dsl, nullptr);
            if (buffer) vkDestroyBuffer(device, buffer, nullptr);
            if (memory) vkFreeMemory(device, memory, nullptr);
            if (cmdPool) vkDestroyCommandPool(device, cmdPool, nullptr);
            vkDestroyDevice(device, nullptr);
        }
        if (instance) vkDestroyInstance(instance, nullptr);
        *this = Impl{};
    }
};

VulkanComputeBackend::VulkanComputeBackend() : d(std::make_unique<Impl>()) {}

VulkanComputeBackend::~VulkanComputeBackend() {
    if (d) d->destroy();
}

bool VulkanComputeBackend::initialize() {
    VkApplicationInfo app{};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "ViewCam";
    app.apiVersion = VK_API_VERSION_1_1;

    VkInstanceCreateInfo ici{};
    ici.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    ici.pApplicationInfo = &app;
    if (vkCreateInstance(&ici, nullptr, &d->instance) != VK_SUCCESS) {
        VC_WARN("Vulkan: vkCreateInstance failed");
        return false;
    }

    uint32_t n = 0;
    vkEnumeratePhysicalDevices(d->instance, &n, nullptr);
    if (n == 0) {
        VC_WARN("Vulkan: no physical devices");
        return false;
    }
    std::vector<VkPhysicalDevice> devs(n);
    vkEnumeratePhysicalDevices(d->instance, &n, devs.data());

    // Prefer a non-NVIDIA device with a compute queue (this is the non-NVIDIA
    // path), else fall back to any compute-capable device.
    auto computeFamily = [](VkPhysicalDevice pd, uint32_t &fam) {
        uint32_t qn = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &qn, nullptr);
        std::vector<VkQueueFamilyProperties> props(qn);
        vkGetPhysicalDeviceQueueFamilyProperties(pd, &qn, props.data());
        for (uint32_t i = 0; i < qn; ++i)
            if (props[i].queueFlags & VK_QUEUE_COMPUTE_BIT) { fam = i; return true; }
        return false;
    };

    VkPhysicalDevice chosen = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties chosenProps{};
    uint32_t fam = 0;
    for (int pass = 0; pass < 2 && chosen == VK_NULL_HANDLE; ++pass) {
        for (auto pd : devs) {
            VkPhysicalDeviceProperties p{};
            vkGetPhysicalDeviceProperties(pd, &p);
            const bool isNvidia = p.vendorID == 0x10DE;
            if (pass == 0 && isNvidia) continue; // first pass: skip NVIDIA
            uint32_t f = 0;
            if (computeFamily(pd, f)) { chosen = pd; chosenProps = p; fam = f; break; }
        }
    }
    if (chosen == VK_NULL_HANDLE) {
        VC_WARN("Vulkan: no compute-capable device");
        return false;
    }
    d->phys = chosen;
    d->queueFamily = fam;
    m_device = std::string(chosenProps.deviceName) + " (" +
               vendorName(chosenProps.vendorID) + ")";

    const float prio = 1.0f;
    VkDeviceQueueCreateInfo qci{};
    qci.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    qci.queueFamilyIndex = fam;
    qci.queueCount = 1;
    qci.pQueuePriorities = &prio;
    VkDeviceCreateInfo dci{};
    dci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    if (vkCreateDevice(d->phys, &dci, nullptr, &d->device) != VK_SUCCESS) {
        VC_WARN("Vulkan: vkCreateDevice failed");
        return false;
    }
    vkGetDeviceQueue(d->device, fam, 0, &d->queue);

    // Host-visible|coherent storage buffer (no staging needed for a proof).
    VkBufferCreateInfo bci{};
    bci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bci.size = Impl::kBytes;
    bci.usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    bci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(d->device, &bci, nullptr, &d->buffer) != VK_SUCCESS)
        return false;

    VkMemoryRequirements req{};
    vkGetBufferMemoryRequirements(d->device, d->buffer, &req);
    VkPhysicalDeviceMemoryProperties mp{};
    vkGetPhysicalDeviceMemoryProperties(d->phys, &mp);
    uint32_t memType = UINT32_MAX;
    const VkMemoryPropertyFlags want =
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    for (uint32_t i = 0; i < mp.memoryTypeCount; ++i)
        if ((req.memoryTypeBits & (1u << i)) &&
            (mp.memoryTypes[i].propertyFlags & want) == want) { memType = i; break; }
    if (memType == UINT32_MAX) {
        VC_WARN("Vulkan: no host-visible memory type");
        return false;
    }
    VkMemoryAllocateInfo mai{};
    mai.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    mai.allocationSize = req.size;
    mai.memoryTypeIndex = memType;
    if (vkAllocateMemory(d->device, &mai, nullptr, &d->memory) != VK_SUCCESS)
        return false;
    vkBindBufferMemory(d->device, d->buffer, d->memory, 0);

    // Descriptor set layout / pool / set (1 storage buffer at binding 0).
    VkDescriptorSetLayoutBinding b{};
    b.binding = 0;
    b.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    b.descriptorCount = 1;
    b.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    VkDescriptorSetLayoutCreateInfo dslci{};
    dslci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dslci.bindingCount = 1;
    dslci.pBindings = &b;
    if (vkCreateDescriptorSetLayout(d->device, &dslci, nullptr, &d->dsl) != VK_SUCCESS)
        return false;

    VkDescriptorPoolSize ps{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1};
    VkDescriptorPoolCreateInfo dpci{};
    dpci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    dpci.maxSets = 1;
    dpci.poolSizeCount = 1;
    dpci.pPoolSizes = &ps;
    if (vkCreateDescriptorPool(d->device, &dpci, nullptr, &d->pool) != VK_SUCCESS)
        return false;
    VkDescriptorSetAllocateInfo dsai{};
    dsai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    dsai.descriptorPool = d->pool;
    dsai.descriptorSetCount = 1;
    dsai.pSetLayouts = &d->dsl;
    if (vkAllocateDescriptorSets(d->device, &dsai, &d->dset) != VK_SUCCESS)
        return false;
    VkDescriptorBufferInfo dbi{d->buffer, 0, Impl::kBytes};
    VkWriteDescriptorSet w{};
    w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet = d->dset;
    w.dstBinding = 0;
    w.descriptorCount = 1;
    w.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    w.pBufferInfo = &dbi;
    vkUpdateDescriptorSets(d->device, 1, &w, 0, nullptr);

    // Shader module + compute pipeline (push constant: element count).
    VkShaderModuleCreateInfo smci{};
    smci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    smci.codeSize = sizeof(kProofCompSpv);
    smci.pCode = kProofCompSpv;
    if (vkCreateShaderModule(d->device, &smci, nullptr, &d->shader) != VK_SUCCESS)
        return false;

    VkPushConstantRange pcr{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(uint32_t)};
    VkPipelineLayoutCreateInfo plci{};
    plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &d->dsl;
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges = &pcr;
    if (vkCreatePipelineLayout(d->device, &plci, nullptr, &d->pipeLayout) != VK_SUCCESS)
        return false;

    VkPipelineShaderStageCreateInfo stage{};
    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stage.module = d->shader;
    stage.pName = "main";
    VkComputePipelineCreateInfo cpci{};
    cpci.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    cpci.stage = stage;
    cpci.layout = d->pipeLayout;
    if (vkCreateComputePipelines(d->device, VK_NULL_HANDLE, 1, &cpci, nullptr,
                                 &d->pipeline) != VK_SUCCESS)
        return false;

    VkCommandPoolCreateInfo cpc{};
    cpc.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    cpc.queueFamilyIndex = fam;
    cpc.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    if (vkCreateCommandPool(d->device, &cpc, nullptr, &d->cmdPool) != VK_SUCCESS)
        return false;

    m_available = true;
    VC_INFO("Vulkan compute device: {}", m_device);
    return true;
}

bool VulkanComputeBackend::runProofOfLife() {
    if (!m_available)
        return false;

    // Seed buffer: data[i] = i.
    void *mapped = nullptr;
    if (vkMapMemory(d->device, d->memory, 0, Impl::kBytes, 0, &mapped) != VK_SUCCESS)
        return false;
    auto *f = static_cast<float *>(mapped);
    for (uint32_t i = 0; i < Impl::kCount; ++i)
        f[i] = static_cast<float>(i);
    vkUnmapMemory(d->device, d->memory);

    VkCommandBufferAllocateInfo cbai{};
    cbai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cbai.commandPool = d->cmdPool;
    cbai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cbai.commandBufferCount = 1;
    VkCommandBuffer cb = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(d->device, &cbai, &cb) != VK_SUCCESS)
        return false;

    VkCommandBufferBeginInfo bi{};
    bi.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(cb, &bi);
    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, d->pipeline);
    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, d->pipeLayout, 0,
                            1, &d->dset, 0, nullptr);
    const uint32_t count = Impl::kCount;
    vkCmdPushConstants(cb, d->pipeLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0,
                       sizeof(uint32_t), &count);
    vkCmdDispatch(cb, (Impl::kCount + 63) / 64, 1, 1);
    vkEndCommandBuffer(cb);

    VkFenceCreateInfo fci{};
    fci.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    VkFence fence = VK_NULL_HANDLE;
    vkCreateFence(d->device, &fci, nullptr, &fence);
    VkSubmitInfo si{};
    si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    si.commandBufferCount = 1;
    si.pCommandBuffers = &cb;
    bool ok = vkQueueSubmit(d->queue, 1, &si, fence) == VK_SUCCESS;
    if (ok)
        ok = vkWaitForFences(d->device, 1, &fence, VK_TRUE, 2'000'000'000ull) == VK_SUCCESS;
    vkDestroyFence(d->device, fence, nullptr);
    vkFreeCommandBuffers(d->device, d->cmdPool, 1, &cb);

    if (ok) {
        if (vkMapMemory(d->device, d->memory, 0, Impl::kBytes, 0, &mapped) != VK_SUCCESS)
            return false;
        f = static_cast<float *>(mapped);
        for (uint32_t i = 0; i < Impl::kCount; ++i) {
            if (f[i] != static_cast<float>(i) * 2.0f + 1.0f) { ok = false; break; }
        }
        vkUnmapMemory(d->device, d->memory);
    }
    return ok;
}

std::unique_ptr<GpuFrameFilter> VulkanComputeBackend::createPassthroughFilter() {
    // Scaffold: real Vulkan compute filters plug in here later.
    return std::make_unique<PassthroughFilter>();
}

#else // !VIEWCAM_HAVE_VULKAN — stub so the selector compiles & falls through

struct VulkanComputeBackend::Impl {};
VulkanComputeBackend::VulkanComputeBackend() = default;
VulkanComputeBackend::~VulkanComputeBackend() = default;
bool VulkanComputeBackend::initialize() { return false; }
bool VulkanComputeBackend::runProofOfLife() { return false; }
std::unique_ptr<GpuFrameFilter> VulkanComputeBackend::createPassthroughFilter() {
    return std::make_unique<PassthroughFilter>();
}

#endif
