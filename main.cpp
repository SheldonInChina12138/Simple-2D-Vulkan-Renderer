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

void InitVulkan() {}

int main() {
    // 初始化 GLFW
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);

    // 创建 Vulkan 实例
    vk::ApplicationInfo appInfo("Lesson 3", 1, "NoEngine", 1, VK_API_VERSION_1_0);
    vk::InstanceCreateInfo createInfo({}, &appInfo);

    auto extensions = glfwGetRequiredInstanceExtensions(&createInfo.enabledExtensionCount);
    createInfo.ppEnabledExtensionNames = extensions;
    vk::UniqueInstance instance = vk::createInstanceUnique(createInfo);

    // 创建 Surface
    VkSurfaceKHR c_surface;
    if (glfwCreateWindowSurface(static_cast<VkInstance>(*instance), window, nullptr, &c_surface) != VK_SUCCESS)
        throw std::runtime_error("failed to create window surface!");
    vk::SurfaceKHR surface = c_surface;

    // 选择物理设备
    auto gpus = instance->enumeratePhysicalDevices();
    auto physicalDevice = gpus[0];

    // 获取队列家族
    auto families = physicalDevice.getQueueFamilyProperties();
    std::optional<uint32_t> graphicsFamily;
    for (uint32_t i = 0; i < families.size(); ++i) {
        if (families[i].queueFlags & vk::QueueFlagBits::eGraphics && physicalDevice.getSurfaceSupportKHR(i, surface)) {
            graphicsFamily = i;
            break;
        }
    }
    if (!graphicsFamily.has_value()) throw std::runtime_error("no suitable queue family found");

    // 创建逻辑设备
    float priority = 1.0f;
    vk::DeviceQueueCreateInfo queueInfo({}, graphicsFamily.value(), 1, &priority);
    //重要！注意设备也需要扩展
    std::vector<const char*> deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    vk::DeviceCreateInfo deviceCreateInfo({}, queueInfo, {}, deviceExtensions);
    vk::UniqueDevice device = physicalDevice.createDeviceUnique(deviceCreateInfo);

    vk::Queue graphicsQueue = device->getQueue(graphicsFamily.value(), 0);

    // 获取 Surface 支持信息
    auto formats = physicalDevice.getSurfaceFormatsKHR(surface);
    auto capabilities = physicalDevice.getSurfaceCapabilitiesKHR(surface);
    auto presentModes = physicalDevice.getSurfacePresentModesKHR(surface);

    vk::SurfaceFormatKHR surfaceFormat = formats[0];
    vk::Extent2D extent = capabilities.currentExtent;

    // 创建 Swapchain
    vk::SwapchainCreateInfoKHR swapchainInfo({}, surface,
        capabilities.minImageCount, surfaceFormat.format, surfaceFormat.colorSpace,
        extent, 1, vk::ImageUsageFlagBits::eColorAttachment,
        vk::SharingMode::eExclusive, {}, vk::SurfaceTransformFlagBitsKHR::eIdentity,
        vk::CompositeAlphaFlagBitsKHR::eOpaque, presentModes[0], VK_TRUE);

    vk::UniqueSwapchainKHR swapchain = device->createSwapchainKHRUnique(swapchainInfo);

    // 获取 Swapchain 图片 & ImageViews
    auto images = device->getSwapchainImagesKHR(*swapchain);
    std::vector<vk::UniqueImageView> imageViews;
    for (auto image : images) {
        vk::ImageViewCreateInfo viewInfo({}, image, vk::ImageViewType::e2D, surfaceFormat.format,
            {}, { vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1 });
        imageViews.push_back(device->createImageViewUnique(viewInfo));
    }

    // 创建 RenderPass
    vk::AttachmentDescription colorAttachment({}, surfaceFormat.format, vk::SampleCountFlagBits::e1,
        vk::AttachmentLoadOp::eClear, vk::AttachmentStoreOp::eStore,
        vk::AttachmentLoadOp::eDontCare, vk::AttachmentStoreOp::eDontCare,
        vk::ImageLayout::eUndefined, vk::ImageLayout::ePresentSrcKHR);

    vk::AttachmentReference colorRef(0, vk::ImageLayout::eColorAttachmentOptimal);

    vk::SubpassDescription subpass({}, vk::PipelineBindPoint::eGraphics);
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;

    vk::RenderPassCreateInfo renderPassInfo({}, 1, &colorAttachment, 1, &subpass);
    vk::UniqueRenderPass renderPass = device->createRenderPassUnique(renderPassInfo);

    // 创建 Framebuffers
    std::vector<vk::UniqueFramebuffer> framebuffers;
    for (auto& view : imageViews) {
        vk::FramebufferCreateInfo fbInfo({}, *renderPass, 1, &*view, extent.width, extent.height, 1);
        framebuffers.push_back(device->createFramebufferUnique(fbInfo));
    }
    
    // ========== 【新增】Uniform Buffer 和 Descriptor Set ==========
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

    // 2. 创建 Descriptor Set Layout
    vk::DescriptorSetLayoutBinding binding(
        0, // binding
        vk::DescriptorType::eUniformBuffer,
        1,
        vk::ShaderStageFlagBits::eFragment
    );
    vk::DescriptorSetLayoutCreateInfo layoutInfo({}, 1, &binding);
    vk::UniqueDescriptorSetLayout descriptorSetLayout = device->createDescriptorSetLayoutUnique(layoutInfo);

    // 3. 创建 Descriptor Pool & Set
    vk::DescriptorPoolSize DpoolSize(vk::DescriptorType::eUniformBuffer, 1);
    vk::DescriptorPoolCreateInfo DpoolInfo({}, 1, 1, &DpoolSize);
    vk::UniqueDescriptorPool descriptorPool = device->createDescriptorPoolUnique(DpoolInfo);

    vk::DescriptorSetAllocateInfo allocSetInfo(*descriptorPool, 1, &*descriptorSetLayout);
    auto descriptorSets = device->allocateDescriptorSetsUnique(allocSetInfo);

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
    auto vertCode = readFile("Shader/test.vert.spv");
    auto fragCode = readFile("Shader/test.frag.spv");
    vk::ShaderModuleCreateInfo vertInfo({}, vertCode.size(), reinterpret_cast<const uint32_t*>(vertCode.data()));
    vk::ShaderModuleCreateInfo fragInfo({}, fragCode.size(), reinterpret_cast<const uint32_t*>(fragCode.data()));
    vk::UniqueShaderModule vertShader = device->createShaderModuleUnique(vertInfo);
    vk::UniqueShaderModule fragShader = device->createShaderModuleUnique(fragInfo);

    vk::PipelineShaderStageCreateInfo vertStage({}, vk::ShaderStageFlagBits::eVertex, *vertShader, "main");
    vk::PipelineShaderStageCreateInfo fragStage({}, vk::ShaderStageFlagBits::eFragment, *fragShader, "main");
    vk::PipelineShaderStageCreateInfo stages[] = { vertStage, fragStage };

    // Pipeline 相关设置
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
    vk::UniquePipelineLayout pipelineLayout = device->createPipelineLayoutUnique(pipelineLayoutInfo);

    vk::GraphicsPipelineCreateInfo pipelineInfo({}, 2, stages);
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
    vk::UniquePipeline pipeline = std::move(pipelineResult.value);

    // 【8】创建 command pool 和 command buffer（等于：GPU“任务纸条”）
    vk::CommandPoolCreateInfo poolInfo({}, graphicsFamily.value());
    auto commandPool = device->createCommandPoolUnique(poolInfo);
    std::vector<vk::UniqueCommandBuffer> commandBuffers =
        device->allocateCommandBuffersUnique({ *commandPool, vk::CommandBufferLevel::ePrimary, (uint32_t)framebuffers.size() });

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

