// Lesson 3: 彩色三角形 + 动态帧循环结构 + Uniform Buffer

#define GLFW_INCLUDE_VULKAN // 自动包含 vulkan.h
#include <GLFW/glfw3.h>
#include <vulkan/vulkan.hpp>

#include <iostream>
#include <fstream>
#include <vector>
#include <optional>
#include <stdexcept>
#include <array>
#include <chrono>
#include <cstring> // memcpy

const int WIDTH = 800;
const int HEIGHT = 600;

// 【Lesson4新增】实例计数（多个三角形）
const int INSTANCE_COUNT = 100;
//-------------------------------------Tools-------------------------------------------
// 读取SPV文件
std::vector<char> readFile(const std::string& filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) throw std::runtime_error("failed to open file!");
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    return buffer;
}
//------------------------------------Process------------------------------------------
//创建窗口(初始化 GLFW，告诉它“我不打算用 OpenGL”)
GLFWwindow* InitWindow(int w, int h, std::string winName){
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    return glfwCreateWindow(w, h, winName.c_str(), nullptr, nullptr);
}
//创建 Vulkan 实例（等于：跟系统打招呼“我要用 Vulkan”）
vk::UniqueInstance InitInstance() {
    vk::ApplicationInfo appInfo("Lesson 3", 1, "NoEngine", 1, VK_API_VERSION_1_0);
    vk::InstanceCreateInfo createInfo({}, &appInfo);

    auto extensions = glfwGetRequiredInstanceExtensions(&createInfo.enabledExtensionCount);//remember to add extentions
    createInfo.ppEnabledExtensionNames = extensions;
    return vk::createInstanceUnique(createInfo);
}

//创建 Surface（vulkan与操作系统窗口(glfw)之间的跑腿小哥，对接作用）
vk::SurfaceKHR InitSurface(
    const vk::UniqueInstance& instance, 
    GLFWwindow* window) 
{
    VkSurfaceKHR c_surface;
    if (glfwCreateWindowSurface(static_cast<VkInstance>(*instance), window, nullptr, &c_surface) != VK_SUCCESS)
        throw std::runtime_error("failed to create window surface!");
    return c_surface;
}

//创建硬件队列家族（画师家族）
std::optional<uint32_t> InitGraphicFamily(
    const vk::PhysicalDevice& physicalDevice, 
    const vk::SurfaceKHR& surface) 
{
    auto families = physicalDevice.getQueueFamilyProperties();
    std::optional<uint32_t> graphicsFamily;
    for (uint32_t i = 0; i < families.size(); ++i) {
        if (families[i].queueFlags & vk::QueueFlagBits::eGraphics && physicalDevice.getSurfaceSupportKHR(i, surface)) {
            graphicsFamily = i;
            break;
        }
    }
    if (!graphicsFamily.has_value()) throw std::runtime_error("no suitable queue family found");
    return graphicsFamily;
}

//逻辑设备：软件层来控制物理设备的中间层，就像个仪表盘一样——仪表盘接受人类的命令，然后控制硬件工作
vk::UniqueDevice InitDevice(
    const std::optional<uint32_t>& graphicsFamily, 
    const vk::PhysicalDevice& physicalDevice) 
{
    float priority = 1.0f;
    vk::DeviceQueueCreateInfo queueInfo({}, graphicsFamily.value(), 1, &priority);
    //重要！注意设备也需要扩展
    std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    vk::DeviceCreateInfo deviceCreateInfo({}, queueInfo, {}, deviceExtensions);
    return physicalDevice.createDeviceUnique(deviceCreateInfo);
}

//传送带：运送framebuffer的工具，用来做多帧缓冲
vk::UniqueSwapchainKHR InitSwapChain(
    const vk::PhysicalDevice& physicalDevice,
    const vk::UniqueDevice& device,
    const vk::SurfaceKHR& surface, 
    const vk::SurfaceCapabilitiesKHR& capabilities, 
    const vk::SurfaceFormatKHR& surfaceFormat, 
    const vk::Extent2D& extent) 
{
    auto presentModes = physicalDevice.getSurfacePresentModesKHR(surface);
    vk::SwapchainCreateInfoKHR swapchainInfo({}, surface,
        capabilities.minImageCount, surfaceFormat.format, surfaceFormat.colorSpace,
        extent, 1, vk::ImageUsageFlagBits::eColorAttachment,
        vk::SharingMode::eExclusive, {}, vk::SurfaceTransformFlagBitsKHR::eIdentity,
        vk::CompositeAlphaFlagBitsKHR::eOpaque, presentModes[0], VK_TRUE);

    return device->createSwapchainKHRUnique(swapchainInfo);
}

//ImageView：传送带上各个画板（framebuffer）的画纸说明书，因为它决定了每张图片的格式、底色、用途（颜色、深度）等基本信息
std::vector<vk::UniqueImageView> InitImageViews(
    const vk::UniqueDevice& device, 
    const vk::UniqueSwapchainKHR& swapchain, 
    const vk::SurfaceFormatKHR& surfaceFormat) 
{
    auto images = device->getSwapchainImagesKHR(*swapchain);
    std::vector<vk::UniqueImageView> imageViews;
    for (auto image : images) {
        vk::ImageViewCreateInfo viewInfo({}, image, vk::ImageViewType::e2D, surfaceFormat.format,
            {}, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
        imageViews.push_back(device->createImageViewUnique(viewInfo));
    }
    return imageViews;
}

//RenderPass：提前规划好的绘画步骤说明书，subpass则是每一步骤。默认带一个subpass.
vk::UniqueRenderPass InitRenderPass(
    const vk::SurfaceFormatKHR& surfaceFormat, 
    const vk::UniqueDevice& device) 
{
    vk::AttachmentDescription colorAttachment({}, surfaceFormat.format, vk::SampleCountFlagBits::e1,
        vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
        vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
        vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR);

    vk::AttachmentReference colorRef(0, vk::ImageLayout::eColorAttachmentOptimal);

    vk::SubpassDescription subpass({}, vk::PipelineBindPoint::eGraphics);
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;

    vk::RenderPassCreateInfo renderPassInfo({}, 1, &colorAttachment, 1, &subpass);
    return device->createRenderPassUnique(renderPassInfo);
}

//Framebuffer
std::vector<vk::UniqueFramebuffer> InitFrameBuffer(
    const std::vector<vk::UniqueImageView>& imageViews,
    const vk::UniqueRenderPass& renderPass,
    const vk::Extent2D& extent,
    const vk::UniqueDevice& device)
{
    std::vector<vk::UniqueFramebuffer> framebuffers;
    for (auto& view : imageViews) {
        vk::FramebufferCreateInfo fbInfo({}, *renderPass, 1, &*view, extent.width, extent.height, 1);
        framebuffers.push_back(device->createFramebufferUnique(fbInfo));
    }
    return framebuffers;
}

//Pipeline
vk::UniquePipeline InitPipeline(
    const vk::Extent2D& extent, 
    const vk::UniqueDevice& device,
    const vk::UniqueRenderPass& renderPass,
    const vk::UniqueDescriptorSetLayout& descriptorSetLayout, 
    std::array<vk::PipelineShaderStageCreateInfo, 2> stages,
    vk::UniquePipelineLayout& pipelineLayout)
{
    vk::PipelineVertexInputStateCreateInfo vertexInput;
    vk::PipelineInputAssemblyStateCreateInfo inputAssembly({}, vk::PrimitiveTopology::eTriangleList);
    vk::Viewport viewport(0.0f, 0.0f, (float)extent.width, (float)extent.height, 0.0f, 1.0f);
    vk::Rect2D scissor({ 0, 0 }, extent);
    vk::PipelineViewportStateCreateInfo viewportState({}, 1, &viewport, 1, &scissor);
    vk::PipelineRasterizationStateCreateInfo rasterizer({}, false, false, vk::PolygonMode::eFill,
        vk::CullModeFlagBits::eBack, vk::FrontFace::eClockwise);
    vk::PipelineMultisampleStateCreateInfo multisampling({}, vk::SampleCountFlagBits::e1);
    vk::PipelineColorBlendAttachmentState blendState(false);
    blendState.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
    vk::PipelineColorBlendStateCreateInfo colorBlending({}, false, vk::LogicOp::eCopy, 1, &blendState);

    // 【修改】Pipeline Layout 现在包含 Descriptor Set
    vk::PipelineLayoutCreateInfo pipelineLayoutInfo({}, 1, &*descriptorSetLayout);
    pipelineLayout = device->createPipelineLayoutUnique(pipelineLayoutInfo);

    vk::GraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages.data();
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &inputAssembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &rasterizer;
    pipelineInfo.pMultisampleState = &multisampling;
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.layout = *pipelineLayout;
    pipelineInfo.renderPass = *renderPass;
    pipelineInfo.subpass = 0;

    auto pipelineResult = device->createGraphicsPipelineUnique({}, pipelineInfo);
    return std::move(pipelineResult.value);
}

//DescriptorSet
std::vector<vk::UniqueDescriptorSet, std::allocator<vk::UniqueDescriptorSet>> InitDescriptorSets(
    const vk::UniqueDevice& device, 
    vk::UniqueDescriptorSetLayout& descriptorSetLayout, 
    vk::UniqueDescriptorPool& descriptorPool
    )
{
    // 2. 创建 Descriptor Set Layout
    vk::DescriptorSetLayoutBinding binding(
        0, // binding
        vk::DescriptorType::eUniformBuffer,
        1,
        vk::ShaderStageFlagBits::eFragment
    );
    vk::DescriptorSetLayoutCreateInfo layoutInfo({}, 1, &binding);
    descriptorSetLayout = device->createDescriptorSetLayoutUnique(layoutInfo);

    // 3. 创建 Descriptor Pool & Set
    vk::DescriptorPoolSize DpoolSize(vk::DescriptorType::eUniformBuffer, 1);
    vk::DescriptorPoolCreateInfo DpoolInfo({}, 1, 1, &DpoolSize);
    descriptorPool = device->createDescriptorPoolUnique(DpoolInfo);

    vk::DescriptorSetAllocateInfo allocSetInfo(*descriptorPool, 1, &*descriptorSetLayout);
    return device->allocateDescriptorSetsUnique(allocSetInfo);
}

//加载shader
std::array<vk::PipelineShaderStageCreateInfo, 2> InitShader(
    const std::string& vertPath, 
    const std::string& fragPath,
    const vk::UniqueDevice& device) 
{
    auto vertCode = readFile(vertPath);
    auto fragCode = readFile(fragPath);
    vk::ShaderModuleCreateInfo vertInfo({}, vertCode.size(), reinterpret_cast<const uint32_t*>(vertCode.data()));
    vk::ShaderModuleCreateInfo fragInfo({}, fragCode.size(), reinterpret_cast<const uint32_t*>(fragCode.data()));
    vk::UniqueShaderModule vertShader = device->createShaderModuleUnique(vertInfo);
    vk::UniqueShaderModule fragShader = device->createShaderModuleUnique(fragInfo);

    vk::PipelineShaderStageCreateInfo vertStage({}, vk::ShaderStageFlagBits::eVertex, *vertShader, "main");
    vk::PipelineShaderStageCreateInfo fragStage({}, vk::ShaderStageFlagBits::eFragment, *fragShader, "main");
    return { vertStage, fragStage };//使用array而非传统数组，不然无法作形参传递
}

int main() {
    //==============Init Vulkan===================
    // 初始化 GLFW
    GLFWwindow* window = InitWindow(WIDTH, HEIGHT, "Gamer");

    // 创建 Vulkan 实例
    vk::UniqueInstance instance = InitInstance();

    // 创建 Surface
    vk::SurfaceKHR surface = InitSurface(instance, window);

    // 选择物理设备
    vk::PhysicalDevice physicalDevice = instance->enumeratePhysicalDevices()[0];

    // 获取队列家族
    std::optional<uint32_t> graphicsFamily = InitGraphicFamily(physicalDevice, surface);

    // 创建逻辑设备
    vk::UniqueDevice device = InitDevice(graphicsFamily, physicalDevice);

    vk::Queue graphicsQueue = device->getQueue(graphicsFamily.value(), 0);

    // 获取 Surface 支持信息
    vk::SurfaceCapabilitiesKHR capabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
    vk::SurfaceFormatKHR surfaceFormat = physicalDevice.getSurfaceFormatsKHR(surface)[0];
    vk::Extent2D extent = capabilities.currentExtent;

    // 创建 Swapchain
    vk::UniqueSwapchainKHR swapchain = InitSwapChain(physicalDevice, device, surface, capabilities, surfaceFormat, extent);

    // 获取 Swapchain 图片 & ImageViews
    std::vector<vk::UniqueImageView> imageViews = InitImageViews(device, swapchain, surfaceFormat);

    // 创建 RenderPass
    vk::UniqueRenderPass renderPass = InitRenderPass(surfaceFormat, device);

    // 创建 Framebuffers
    std::vector<vk::UniqueFramebuffer> framebuffers = InitFrameBuffer(imageViews, renderPass, extent, device);
    
    // =========vertex相关的东西先写在这里吧===========
    // 1. 创建 Uniform Buffer
    vk::BufferCreateInfo bufferInfo(
        {},
        sizeof(float) * 4,  // 传递一个 vec4 颜色
        vk::BufferUsageFlagBits::eUniformBuffer,
        vk::SharingMode::eExclusive
    );
    vk::UniqueBuffer uniformBuffer = device->createBufferUnique(bufferInfo);

    // 分配内存
    auto memRequirements = device->getBufferMemoryRequirements(*uniformBuffer);
    auto memProperties = physicalDevice.getMemoryProperties();
    uint32_t memoryTypeIndex = -1;
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) {
        if ((memRequirements.memoryTypeBits & (1 << i)) &&
            (memProperties.memoryTypes[i].propertyFlags & (vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent))) {
            memoryTypeIndex = i;
            break;
        }
    }
    if (memoryTypeIndex == -1) throw std::runtime_error("failed to find suitable memory type!");

    vk::MemoryAllocateInfo allocInfo(memRequirements.size, memoryTypeIndex);
    vk::UniqueDeviceMemory uniformBufferMemory = device->allocateMemoryUnique(allocInfo);
    device->bindBufferMemory(*uniformBuffer, *uniformBufferMemory, 0);

    // 初始数据（红色）
    void* data;
    device->mapMemory(*uniformBufferMemory, 0, sizeof(float) * 4, {}, &data);
    float initialColor[4] = { 1.0f, 0.0f, 0.0f, 1.0f };
    memcpy(data, initialColor, sizeof(initialColor));
    device->unmapMemory(*uniformBufferMemory);

    vk::UniqueDescriptorSetLayout descriptorSetLayout;
    vk::UniqueDescriptorPool descriptorPool;
    std::vector<vk::UniqueDescriptorSet, std::allocator<vk::UniqueDescriptorSet>> descriptorSets = InitDescriptorSets(device, descriptorSetLayout, descriptorPool);

    // 4. 绑定 Uniform Buffer 到 Descriptor Set
    vk::DescriptorBufferInfo bufferDescInfo(*uniformBuffer, 0, sizeof(float) * 4);
    vk::WriteDescriptorSet descriptorWrite(
        *descriptorSets[0],
        0, 0, 1,
        vk::DescriptorType::eUniformBuffer,
        nullptr,
        &bufferDescInfo
    );
    device->updateDescriptorSets(descriptorWrite, nullptr);
    //-------------------
    // 加载 Shader
    std::array<vk::PipelineShaderStageCreateInfo, 2> stages = InitShader("Shader/test.vert.spv", "Shader/test.frag.spv", device);

    // Pipeline 相关设置
    vk::UniquePipelineLayout pipelineLayout;//将要引用传递进InitPipeline
    vk::UniquePipeline pipeline = InitPipeline(extent, device, renderPass, descriptorSetLayout, stages, pipelineLayout);
    
    // 【8】创建 command pool 和 command buffer（等于：GPU“任务纸条”）
    vk::CommandPoolCreateInfo poolInfo({}, graphicsFamily.value());
    auto commandPool = device->createCommandPoolUnique(poolInfo);
    std::vector<vk::UniqueCommandBuffer> commandBuffers =
        device->allocateCommandBuffersUnique({ *commandPool, vk::CommandBufferLevel::ePrimary, (uint32_t)framebuffers.size() });

    //提前录制commandbuffer阶段
    for (size_t i = 0; i < commandBuffers.size(); ++i) {
        auto& cmd = commandBuffers[i];
        cmd->begin({ vk::CommandBufferUsageFlagBits::eSimultaneousUse });
        vk::ClearValue clearColor(std::array<float, 4>{0.1f, 0.1f, 0.1f, 1.0f});
        vk::RenderPassBeginInfo rpBegin(*renderPass, *framebuffers[i], { {0,0}, extent }, clearColor);
        cmd->beginRenderPass(rpBegin, vk::SubpassContents::eInline);
        cmd->bindPipeline(vk::PipelineBindPoint::eGraphics, *pipeline);
        cmd->bindDescriptorSets(  // 【新增】绑定 Descriptor Set
            vk::PipelineBindPoint::eGraphics,
            *pipelineLayout,
            0, 1, &*descriptorSets[0],
            0, nullptr
        );
        cmd->draw(3, INSTANCE_COUNT, 0, 0);
        cmd->endRenderPass();
        cmd->end();
    }

    // [Lesson3] 帧循环
    vk::SemaphoreCreateInfo semInfo;
    auto imageAvailable = device->createSemaphoreUnique(semInfo);
    auto renderFinished = device->createSemaphoreUnique(semInfo);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // 【新增】动态更新 Uniform 数据（颜色随时间变化）
        
        device->mapMemory(*uniformBufferMemory, 0, sizeof(float) * 4, {}, &data);
        float time = glfwGetTime();
        float dynamicColor[4] = { sin(time) * 0.5f + 0.5f, 0.0f, cos(time) * 0.5f + 0.5f, 1.0f };
        
        memcpy(data, dynamicColor, sizeof(dynamicColor));
        device->unmapMemory(*uniformBufferMemory);

        uint32_t imageIndex = device->acquireNextImageKHR(*swapchain, UINT64_MAX, *imageAvailable).value;
        auto waitStage = vk::PipelineStageFlagBits::eColorAttachmentOutput;

        std::array<vk::Semaphore, 1> waitSemaphores = { *imageAvailable };
        std::array<vk::PipelineStageFlags, 1> waitStages = { waitStage };
        std::array<vk::CommandBuffer, 1> commandBuffersToSubmit = { *commandBuffers[imageIndex] };

        vk::SubmitInfo submitInfo = {};
        submitInfo.setWaitSemaphores(waitSemaphores)
            .setWaitDstStageMask(waitStages)
            .setCommandBuffers(commandBuffersToSubmit);
        graphicsQueue.submit(submitInfo, nullptr);

        vk::PresentInfoKHR presentInfo(1, &*renderFinished, 1, &*swapchain, &imageIndex);
        graphicsQueue.presentKHR(presentInfo);

        device->waitIdle();
    }

    device->waitIdle();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

