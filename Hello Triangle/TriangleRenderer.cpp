#include "TriangleRenderer.h"
#include <fstream>

static std::vector<char> readFile(const std::string& filename) {
	std::ifstream file(filename, std::ios::ate | std::ios::binary);

	if (!file.is_open()) {
		throw std::runtime_error("failed to open file!");
	}

	size_t fileSize = (size_t)file.tellg();
	std::vector<char> buffer(fileSize);

	file.seekg(0);
	file.read(buffer.data(), fileSize);

	file.close();

	return buffer;
}

static float vertexData[] = { // Y up, front = CCW
	 0.0f,   -0.5f,   1.0f, 0.0f, 0.0f,
	-0.5f,    0.5f,   0.0f, 1.0f, 0.0f,
	 0.5f,    0.5f,   0.0f, 0.0f, 1.0f
};

TriangleRenderer::TriangleRenderer(QVulkanWindow* window)
	:window_(window)
{
}

void TriangleRenderer::initResources()
{
	vk::Device device = window_->device();
	const int concurrentFrameCount = window_->concurrentFrameCount();
	vk::PhysicalDeviceLimits limits = window_->physicalDeviceProperties()->limits; // 获取物理设备限制

	// 创建顶点缓冲区
	vk::BufferCreateInfo vertexBufferInfo;
	vertexBufferInfo.usage = vk::BufferUsageFlagBits::eVertexBuffer;
	vertexBufferInfo.size = sizeof(vertexData);
	vertexBuffer_ = device.createBuffer(vertexBufferInfo); // 创建缓冲区对象
	// 获取缓冲区内存需求
	vk::MemoryRequirements vertexMemReq = device.getBufferMemoryRequirements(vertexBuffer_);
	// 分配信息，指定大小与可主机访问的内存类型索引
	vk::MemoryAllocateInfo vertexMemAllocInfo(vertexMemReq.size, window_->hostVisibleMemoryIndex());
	vertexDevMemory_ = device.allocateMemory(vertexMemAllocInfo);
	device.bindBufferMemory(vertexBuffer_, vertexDevMemory_, 0); // 绑定内存到缓冲区

	// 初始化顶点数据 // 映射内存到主机地址空间
	uint8_t* vertexBufferMemPtr = (uint8_t*)device.mapMemory(vertexDevMemory_, 0, vertexMemReq.size);
	memcpy(vertexBufferMemPtr, vertexData, sizeof(vertexData));	// 将顶点数据复制到映射后的内存
	device.unmapMemory(vertexDevMemory_);

	vk::GraphicsPipelineCreateInfo piplineInfo;
	piplineInfo.stageCount = 2;

	auto vertShaderCode = readFile("../Hello Triangle/vert.spv");
	auto fragShaderCode = readFile("../Hello Triangle/frag.spv");

	// 创建着色器模块
	vk::ShaderModuleCreateInfo shaderInfo;
	shaderInfo.codeSize = vertShaderCode.size();
	shaderInfo.pCode = reinterpret_cast<uint32_t*>(vertShaderCode.data());
	vk::ShaderModule vertShader = device.createShaderModule(shaderInfo);

	shaderInfo.codeSize = fragShaderCode.size();
	shaderInfo.pCode = reinterpret_cast<uint32_t*>(fragShaderCode.data());
	vk::ShaderModule fragShader = device.createShaderModule(shaderInfo);
	
	// 设置管线着色器阶段信息
	vk::PipelineShaderStageCreateInfo piplineShaderStage[2];
	piplineShaderStage[0].stage = vk::ShaderStageFlagBits::eVertex;
	piplineShaderStage[0].module = vertShader;
	piplineShaderStage[0].pName = "main";
	piplineShaderStage[1].stage = vk::ShaderStageFlagBits::eFragment;
	piplineShaderStage[1].module = fragShader;
	piplineShaderStage[1].pName = "main";
	piplineInfo.pStages = piplineShaderStage;

	// 设置管线顶点输入布局
	vk::VertexInputBindingDescription vertexBindingDesc;
	vertexBindingDesc.binding = 0;
	vertexBindingDesc.stride = 5 * sizeof(float);
	vertexBindingDesc.inputRate = vk::VertexInputRate::eVertex;

	vk::VertexInputAttributeDescription vertexAttrDesc[2];
	vertexAttrDesc[0].binding = 0;
	vertexAttrDesc[0].location = 0;
	vertexAttrDesc[0].format = vk::Format::eR32G32Sfloat;
	vertexAttrDesc[0].offset = 0;								// position
	vertexAttrDesc[1].binding = 0;
	vertexAttrDesc[1].location = 1;
	vertexAttrDesc[1].format = vk::Format::eR32G32B32Sfloat;
	vertexAttrDesc[1].offset = 2 * sizeof(float);				// color

	vk::PipelineVertexInputStateCreateInfo vertexInputState({}, 1, &vertexBindingDesc, 2, vertexAttrDesc);
	piplineInfo.pVertexInputState = &vertexInputState;

	// 设置管线装配状态
	vk::PipelineInputAssemblyStateCreateInfo vertexAssemblyState({}, vk::PrimitiveTopology::eTriangleList);
	piplineInfo.pInputAssemblyState = &vertexAssemblyState;

	// 设置管线视口和裁剪矩形状态
	vk::PipelineViewportStateCreateInfo viewportState;
	viewportState.viewportCount = 1;	// 仅使用一个视口
	viewportState.scissorCount = 1;		// 仅使用一个裁剪矩形
	piplineInfo.pViewportState = &viewportState;

	// 设置管线光栅化状态
	vk::PipelineRasterizationStateCreateInfo rasterizationState;
	rasterizationState.polygonMode = vk::PolygonMode::eFill;			// 使用填充模式
	rasterizationState.cullMode = vk::CullModeFlagBits::eNone;			// 后面剔除
	rasterizationState.frontFace = vk::FrontFace::eCounterClockwise;	// 逆时针为正面
	rasterizationState.lineWidth = 1.0f;
	piplineInfo.pRasterizationState = &rasterizationState;

	// 设置管线多重采样状态
	vk::PipelineMultisampleStateCreateInfo MSState;
	MSState.rasterizationSamples = (vk::SampleCountFlagBits)window_->sampleCountFlagBits();
	piplineInfo.pMultisampleState = &MSState;

	// 设置管线深度/模板测试及混合状态
	vk::PipelineDepthStencilStateCreateInfo DSState;
	DSState.depthTestEnable = true;
	DSState.depthWriteEnable = true;
	DSState.depthCompareOp = vk::CompareOp::eLessOrEqual;
	piplineInfo.pDepthStencilState = &DSState;

	// 设置管线颜色混合状态
	vk::PipelineColorBlendStateCreateInfo colorBlendState;
	colorBlendState.attachmentCount = 1;
	vk::PipelineColorBlendAttachmentState colorBlendAttachmentState;
	colorBlendAttachmentState.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
	colorBlendState.pAttachments = &colorBlendAttachmentState;
	piplineInfo.pColorBlendState = &colorBlendState;

	// 设置管线动态状态
	vk::PipelineDynamicStateCreateInfo dynamicState;
	vk::DynamicState dynamicEnables[] = { vk::DynamicState::eViewport ,vk::DynamicState::eScissor };
	dynamicState.dynamicStateCount = 2;
	dynamicState.pDynamicStates = dynamicEnables;
	piplineInfo.pDynamicState = &dynamicState;

	// 创建管线布局
	vk::PipelineLayoutCreateInfo piplineLayoutInfo;
	piplineLayout_ = device.createPipelineLayout(piplineLayoutInfo);
	piplineInfo.layout = piplineLayout_;

	piplineInfo.renderPass = window_->defaultRenderPass();

	piplineCache_ = device.createPipelineCache(vk::PipelineCacheCreateInfo());
	pipline_ = device.createGraphicsPipeline(piplineCache_, piplineInfo).value;

	device.destroyShaderModule(vertShader);
	device.destroyShaderModule(fragShader);
}

void TriangleRenderer::initSwapChainResources()
{
}

void TriangleRenderer::releaseSwapChainResources()
{
}

void TriangleRenderer::releaseResources() {
	vk::Device device = window_->device();
	device.destroyPipeline(pipline_);
	device.destroyPipelineCache(piplineCache_);
	device.destroyPipelineLayout(piplineLayout_);
	device.destroyBuffer(vertexBuffer_);
	device.freeMemory(vertexDevMemory_);
}

void TriangleRenderer::startNextFrame() {
	vk::Device device = window_->device();
	vk::CommandBuffer cmdBuffer = window_->currentCommandBuffer();
	const QSize size = window_->swapChainImageSize();

	vk::ClearValue clearValues[3] = {
		vk::ClearColorValue(std::array<float,4>{1.0f,0.0f,0.0f,1.0f }),
		vk::ClearDepthStencilValue(1.0f,0),
		vk::ClearColorValue(std::array<float,4>{ 0.0f,0.5f,0.9f,1.0f }),
	};

	vk::RenderPassBeginInfo beginInfo;
	beginInfo.renderPass = window_->defaultRenderPass();
	beginInfo.framebuffer = window_->currentFramebuffer();
	beginInfo.renderArea.extent.width = size.width();
	beginInfo.renderArea.extent.height = size.height();
	beginInfo.clearValueCount = window_->sampleCountFlagBits() > VK_SAMPLE_COUNT_1_BIT ? 3 : 2;
	beginInfo.pClearValues = clearValues;

	cmdBuffer.beginRenderPass(beginInfo, vk::SubpassContents::eInline);

	vk::Viewport viewport;
	viewport.x = 0;
	viewport.y = 0;
	viewport.width = size.width();
	viewport.height = size.height();

	viewport.minDepth = 0;
	viewport.maxDepth = 1;
	cmdBuffer.setViewport(0, viewport);

	vk::Rect2D scissor;
	scissor.offset.x = scissor.offset.y = 0;
	scissor.extent.width = size.width();
	scissor.extent.height = size.height();
	cmdBuffer.setScissor(0, scissor);

	cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipline_);

	cmdBuffer.bindVertexBuffers(0, vertexBuffer_, { 0 });

	cmdBuffer.draw(3, 1, 0, 0);

	cmdBuffer.endRenderPass();

	window_->frameReady();
	window_->requestUpdate();
}