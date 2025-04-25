#include "Tools.h" // memcpy

const int WIDTH = 800;
const int HEIGHT = 800;
const int INSTANCE_COUNT = 5;  // 【修改】改为单个正方形

// 顶点数据结构
struct Vertex {
    float pos[2];
    float color[3];
};

// 【新增】实例数据：存储每个实例的偏移位置
struct InstanceData {
    float offset[2];
};

// 正方形顶点和索引数据
const std::vector<Vertex> vertices = {
    {{-0.1f, -0.1f}, {1.0f, 0.0f, 0.0f}}, // 左下
    {{0.1f, -0.1f}, {1.0f, 0.0f, 0.0f}},  // 右下
    {{0.1f, 0.1f}, {1.0f, 0.0f, 0.0f}},   // 右上
    {{-0.1f, 0.1f}, {1.0f, 0.0f, 0.0f}}   // 左上
};
const std::vector<uint16_t> indices = { 0, 1, 2, 2, 3, 0 };

// 【新增】实例数据数组
const std::vector<InstanceData> instanceData = {
    {{-0.4f, -0.4f}}, {{0.4f, -0.4f}}, {{0.0f, 0.4f}},
    {{-0.2f, 0.0f}}, {{0.2f, 0.0f}}
};

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

    //Uniform缓冲
    auto [uniformBuffer, uniformBufferMemory] = createBuffer(device, physicalDevice, sizeof(float) * 4, vk::BufferUsageFlagBits::eUniformBuffer);
    
    //顶点缓冲
    auto [vertexBuffer, vertexBufferMemory] = createBuffer(device,physicalDevice,sizeof(vertices[0]) * vertices.size(),vk::BufferUsageFlagBits::eVertexBuffer, vertices.data());

    // 索引缓冲
    auto [indexBuffer, indexBufferMemory] = createBuffer(device, physicalDevice, sizeof(indices[0]) * indices.size(), vk::BufferUsageFlagBits::eIndexBuffer, indices.data());
    
    // 【新增】创建实例缓冲
    auto [instanceBuffer, instanceBufferMemory] = createBuffer(device, physicalDevice,
        sizeof(instanceData[0]) * instanceData.size(),
        vk::BufferUsageFlagBits::eVertexBuffer,
        instanceData.data());

    // 【修改】顶点绑定描述：添加实例数据绑定
    std::array<vk::VertexInputBindingDescription, 2> bindingDesc = {
        vk::VertexInputBindingDescription(0, sizeof(Vertex), vk::VertexInputRate::eVertex), // 顶点数据
        vk::VertexInputBindingDescription(1, sizeof(InstanceData), vk::VertexInputRate::eInstance) // 实例数据
    };

    // 【修改】顶点属性描述：添加实例偏移属性
    std::array<vk::VertexInputAttributeDescription, 3> attrDesc = {
        vk::VertexInputAttributeDescription(0, 0, vk::Format::eR32G32Sfloat, offsetof(Vertex, pos)),  // 顶点位置
        vk::VertexInputAttributeDescription(1, 0, vk::Format::eR32G32B32Sfloat, offsetof(Vertex, color)), // 顶点颜色
        vk::VertexInputAttributeDescription(2, 1, vk::Format::eR32G32Sfloat, offsetof(InstanceData, offset)) // 实例偏移
    };

    vk::PipelineVertexInputStateCreateInfo vertexInput({}, bindingDesc, attrDesc);

    //==================================================
    vk::UniqueDescriptorSetLayout descriptorSetLayout;
    vk::UniqueDescriptorPool descriptorPool;
    std::vector<vk::UniqueDescriptorSet, std::allocator<vk::UniqueDescriptorSet>> descriptorSets = InitDescriptorSets(device, descriptorSetLayout, descriptorPool);

    // 4. 绑定 Uniform Buffer 到 Descriptor Set
    vk::DescriptorBufferInfo bufferDescInfo(*uniformBuffer, 0, sizeof(float) * 4);
    vk::WriteDescriptorSet descriptorWrite(*descriptorSets[0], 0, 0, 1, vk::DescriptorType::eUniformBuffer, nullptr, &bufferDescInfo);
    device->updateDescriptorSets(descriptorWrite, nullptr);
    
    //-------------------加载 Shader-----------------
    //注意，shaderModule必须保留至pipeline创建结束
    vk::UniqueShaderModule vertShader;
    vk::UniqueShaderModule fragShader;
    std::array<vk::PipelineShaderStageCreateInfo, 2> stages = InitShader("Shader/test.vert.spv", "Shader/test.frag.spv", vertShader, fragShader, device);

    // Pipeline 相关设置
    vk::UniquePipelineLayout pipelineLayout;//将要引用传递进InitPipeline
    vk::UniquePipeline pipeline = InitPipeline(extent, device, renderPass, vertexInput, descriptorSetLayout, stages, pipelineLayout);

    // 创建 command pool 和 command buffer
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
        cmd->bindDescriptorSets(
            vk::PipelineBindPoint::eGraphics,
            *pipelineLayout,
            0, 1, &*descriptorSets[0],
            0, nullptr
        );
        // 绘制命令改为使用索引绘制
        cmd->bindVertexBuffers(0, { *vertexBuffer, *instanceBuffer }, { 0, 0 });
        cmd->bindIndexBuffer(*indexBuffer, 0, vk::IndexType::eUint16);
        cmd->drawIndexed(static_cast<uint32_t>(indices.size()), INSTANCE_COUNT, 0, 0, 0);
        cmd->draw(3, INSTANCE_COUNT, 0, 0);
        cmd->endRenderPass();
        cmd->end();
    }

    // 帧循环
    vk::SemaphoreCreateInfo semInfo;
    auto imageAvailable = device->createSemaphoreUnique(semInfo);
    auto renderFinished = device->createSemaphoreUnique(semInfo);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

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

