#include "TriangleRenderer.h"
#include <fstream>
#include <QTime>

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
	 0.0f,  -0.5f,   1.0f, 0.0f, 0.0f,
	-0.5f,   0.5f,   0.0f, 1.0f, 0.0f,
	 0.5f,   0.5f,   0.0f, 0.0f, 1.0f
};


TriangleRenderer::TriangleRenderer(QVulkanWindow* window)
	:window_(window)
{
	QList<int> sampleCounts = window->supportedSampleCounts();
	if (!sampleCounts.isEmpty()) {
		window->setSampleCount(sampleCounts.back());
	}
}


void TriangleRenderer::initResources()
{
	vk::Device device = window_->device();

	const int concurrentFrameCount = window_->concurrentFrameCount();
	vk::PhysicalDeviceLimits limits = window_->physicalDeviceProperties()->limits;

	vk::BufferCreateInfo vertexBufferInfo;
	vertexBufferInfo.usage = vk::BufferUsageFlagBits::eVertexBuffer;
	vertexBufferInfo.size = sizeof(vertexData);
	vertexBuffer_ = device.createBuffer(vertexBufferInfo);
	vk::MemoryRequirements vertexMemReq = device.getBufferMemoryRequirements(vertexBuffer_);
	vk::MemoryAllocateInfo vertexMemAllocInfo(vertexMemReq.size, window_->hostVisibleMemoryIndex());
	vertexDevMemory_ = device.allocateMemory(vertexMemAllocInfo);
	device.bindBufferMemory(vertexBuffer_, vertexDevMemory_, 0);
	uint8_t* vertexBufferMemPtr = (uint8_t*)device.mapMemory(vertexDevMemory_, 0, vertexMemReq.size);
	memcpy(vertexBufferMemPtr, vertexData, sizeof(vertexData));
	device.unmapMemory(vertexDevMemory_);

	vk::DescriptorPoolSize descPoolSize(vk::DescriptorType::eUniformBuffer, (uint32_t)concurrentFrameCount);
	vk::DescriptorPoolCreateInfo descPoolInfo;
	descPoolInfo.maxSets = concurrentFrameCount;
	descPoolInfo.poolSizeCount = 1;
	descPoolInfo.pPoolSizes = &descPoolSize;
	descPool_ = device.createDescriptorPool(descPoolInfo);

	vk::DescriptorSetLayoutBinding layoutBinding;
	layoutBinding.binding = 0;
	layoutBinding.descriptorType = vk::DescriptorType::eUniformBuffer;
	layoutBinding.descriptorCount = 1;
	layoutBinding.stageFlags = vk::ShaderStageFlagBits::eVertex;
	layoutBinding.pImmutableSamplers = nullptr;

	vk::DescriptorSetLayoutCreateInfo descLayoutInfo;
	descLayoutInfo.pNext = nullptr;
	descLayoutInfo.flags = {};
	descLayoutInfo.bindingCount = 1;
	descLayoutInfo.pBindings = &layoutBinding;

	descSetLayout_ = device.createDescriptorSetLayout(descLayoutInfo);

	for (int i = 0; i < concurrentFrameCount; ++i) {
		vk::DescriptorSetAllocateInfo descSetAllocInfo(descPool_, 1, &descSetLayout_);
		descSet_[i] = device.allocateDescriptorSets(descSetAllocInfo).front();
	}

	vk::GraphicsPipelineCreateInfo piplineInfo;
	piplineInfo.stageCount = 2;

	auto vertShaderCode = readFile("./triangles_vert.spv");
	auto fragShaderCode = readFile("./triangles_frag.spv");
	
	vk::ShaderModuleCreateInfo shaderInfo;
	shaderInfo.codeSize = vertShaderCode.size();
	shaderInfo.pCode = reinterpret_cast<uint32_t*>(vertShaderCode.data());
	vk::ShaderModule vertShader = device.createShaderModule(shaderInfo);
	
	shaderInfo.codeSize = fragShaderCode.size();;
	shaderInfo.pCode = reinterpret_cast<uint32_t*>(fragShaderCode.data());
	vk::ShaderModule fragShader = device.createShaderModule(shaderInfo);

	vk::PipelineShaderStageCreateInfo piplineShaderStage[2];
	piplineShaderStage[0].stage = vk::ShaderStageFlagBits::eVertex;
	piplineShaderStage[0].module = vertShader;
	piplineShaderStage[0].pName = "main";
	piplineShaderStage[1].stage = vk::ShaderStageFlagBits::eFragment;
	piplineShaderStage[1].module = fragShader;
	piplineShaderStage[1].pName = "main";
	piplineInfo.pStages = piplineShaderStage;

	vk::VertexInputBindingDescription vertexBindingDesc(0, 5 * sizeof(float), vk::VertexInputRate::eVertex);

	vk::VertexInputAttributeDescription vertexAttrDesc[] = {
		{0,0,vk::Format::eR32G32Sfloat,0},
		{1,0,vk::Format::eR32G32B32Sfloat,2 * sizeof(float)}
	};

	vk::PipelineVertexInputStateCreateInfo vertexInputState({}, 1, &vertexBindingDesc, 2, vertexAttrDesc);
	piplineInfo.pVertexInputState = &vertexInputState;

	vk::PipelineInputAssemblyStateCreateInfo vertexAssemblyState({}, vk::PrimitiveTopology::eTriangleList);
	piplineInfo.pInputAssemblyState = &vertexAssemblyState;

	vk::PipelineViewportStateCreateInfo viewportState;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;
	piplineInfo.pViewportState = &viewportState;

	vk::PipelineRasterizationStateCreateInfo rasterizationState;
	rasterizationState.polygonMode = vk::PolygonMode::eFill;
	rasterizationState.cullMode = vk::CullModeFlagBits::eNone;
	rasterizationState.frontFace = vk::FrontFace::eClockwise;
	rasterizationState.lineWidth = 1.0f;
	piplineInfo.pRasterizationState = &rasterizationState;

	vk::PipelineMultisampleStateCreateInfo MSState;
	MSState.rasterizationSamples = (VULKAN_HPP_NAMESPACE::SampleCountFlagBits)window_->sampleCountFlagBits();
	piplineInfo.pMultisampleState = &MSState;

	// ---------------------------------------------------------------------------------------
	vk::PipelineDepthStencilStateCreateInfo DSState;
	DSState.stencilTestEnable = true;
	DSState.back.compareOp = vk::CompareOp::eAlways;		// 设置背面模板测试比较操作为"始终通过"
	DSState.back.depthFailOp = vk::StencilOp::eKeep;		// 当模板测试失败且深度测试通过时，保留现有模板值
	DSState.back.failOp = vk::StencilOp::eKeep;				// 当模板测试失败且深度测试未通过时，保留现有模板值
	DSState.back.passOp = vk::StencilOp::eReplace;			// 当模板测试和深度测试均通过时，将模板值替换为参考值
	DSState.back.compareMask = 0xff;
	DSState.back.writeMask = 0xff;
	DSState.back.reference = 1;								// 设置模板参考值为1
	DSState.front = DSState.back;							// 将背面的模板测试参数设置同时应用于正面
	piplineInfo.pDepthStencilState = &DSState;		
	// ---------------------------------------------------------------------------------------
	vk::PipelineColorBlendStateCreateInfo colorBlendState;
	colorBlendState.attachmentCount = 1;
	vk::PipelineColorBlendAttachmentState colorBlendAttachmentState;
	colorBlendAttachmentState.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
	colorBlendState.pAttachments = &colorBlendAttachmentState;
	piplineInfo.pColorBlendState = &colorBlendState;

	vk::PipelineDynamicStateCreateInfo dynamicState;
	vk::DynamicState dynamicEnables[] = { vk::DynamicState::eViewport ,vk::DynamicState::eScissor };
	dynamicState.dynamicStateCount = 2;
	dynamicState.pDynamicStates = dynamicEnables;
	piplineInfo.pDynamicState = &dynamicState;

	vk::PipelineLayoutCreateInfo piplineLayoutInfo;
	piplineLayoutInfo.setLayoutCount = 1;
	piplineLayoutInfo.pSetLayouts = &descSetLayout_;
	pushConstant_.offset = 0;
	pushConstant_.size = sizeof(float);
	pushConstant_.stageFlags = vk::ShaderStageFlagBits::eVertex;

	piplineLayoutInfo.pushConstantRangeCount = 1;
	piplineLayoutInfo.pPushConstantRanges = &pushConstant_;
	piplineLayout_ = device.createPipelineLayout(piplineLayoutInfo);

	piplineInfo.layout = piplineLayout_;
	piplineInfo.renderPass = window_->defaultRenderPass();

	piplineCache_ = device.createPipelineCache(vk::PipelineCacheCreateInfo());
	pipline_ = device.createGraphicsPipeline(piplineCache_, piplineInfo).value;

	// ---------------------------------------------------------------------------------------------
	DSState.back.compareOp = vk::CompareOp::eNotEqual;		// 更改背面模板测试比较操作为"不等于"
	DSState.front.compareOp = vk::CompareOp::eNotEqual;		// 更改正面模板测试比较操作为"不等于"
	outlinePipline_ = device.createGraphicsPipeline(piplineCache_, piplineInfo).value;

	// ---------------------------------------------------------------------------------------------
	device.destroyShaderModule(vertShader);
	device.destroyShaderModule(fragShader);
}

void TriangleRenderer::initSwapChainResources()
{

}

void TriangleRenderer::releaseSwapChainResources()
{

}

void TriangleRenderer::releaseResources()
{
	vk::Device device = window_->device();
	device.destroyPipeline(pipline_);
	device.destroyPipelineCache(piplineCache_);
	device.destroyPipelineLayout(piplineLayout_);
	device.destroyDescriptorSetLayout(descSetLayout_);
	device.destroyDescriptorPool(descPool_);
	device.destroyBuffer(vertexBuffer_);
	device.freeMemory(vertexDevMemory_);
}

void TriangleRenderer::startNextFrame()
{
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

	cmdBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, piplineLayout_, 0, 1, &descSet_[window_->currentFrame()], 0, nullptr);

	cmdBuffer.bindVertexBuffers(0, vertexBuffer_, { 0 });

	cmdBuffer.pushConstants<float>(piplineLayout_, pushConstant_.stageFlags, 0, { 1.0f });
	cmdBuffer.draw(3, 1, 0, 0);

	cmdBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, outlinePipline_);
	cmdBuffer.pushConstants<float>(piplineLayout_, pushConstant_.stageFlags, 0, { 1.2f });
	cmdBuffer.draw(3, 1, 0, 0);

	cmdBuffer.endRenderPass();

	window_->frameReady();
	window_->requestUpdate();
}