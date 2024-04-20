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
	-0.5f,   0.5f,   0.0f, 1.0f, 0.0f,
	 0.5f,   0.5f,   0.0f, 0.0f, 1.0f
};

TriangleRenderer::TriangleRenderer(QVulkanWindow* window)
	:window_(window)
{
	QList<int> sampleCounts = window_->supportedSampleCounts();
	if (!sampleCounts.isEmpty()) {
		window_->setSampleCount(sampleCounts.back());
	}
}

void TriangleRenderer::initResources()
{
	vk::Device device = window_->device();
	const int concurrentFrameCount = window_->concurrentFrameCount();
	vk::PhysicalDeviceLimits limts = window_->physicalDeviceProperties()->limits;

	vk::BufferCreateInfo vertexBufferInfo;
	vertexBufferInfo.usage = vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst;
	vertexBufferInfo.size = sizeof(vertexData);
	vertexBuffer_ = device.createBuffer(vertexBufferInfo);
	vk::MemoryRequirements vertexMemReq = device.getBufferMemoryRequirements(vertexBuffer_);
	vk::MemoryAllocateInfo  vertexMemAllocInfo(vertexMemReq.size, window_->deviceLocalMemoryIndex());
	vertexDevMemory_ = device.allocateMemory(vertexMemAllocInfo);
	device.bindBufferMemory(vertexBuffer_, vertexDevMemory_, 0);

	// --------------------------------------------------------------------------------------------------------
	// 这种做法常用于将主机端（CPU）的数据高效地传输到设备端（GPU），特别是当目标缓冲区不支持或不适合直接由CPU写入时（如仅支持设备本地访问的内存）。Staging Buffer充当了数据从主机内存到设备内存之间的桥梁。
	// 临时缓冲区（Staging Buffer），并将其用于将顶点数据（vertexData）从主机内存（如CPU RAM）传输到设备内存（如GPU VRAM），以便在图形渲染过程中使用。
	vk::BufferCreateInfo stagingBufferInfo;
	stagingBufferInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;	// 设置该缓冲区的主要用途为“传输源”，即它将作为数据从主机内存到设备内存传输过程中的源。
	stagingBufferInfo.size = sizeof(vertexData);
	vk::Buffer stagingBuffer = device.createBuffer(stagingBufferInfo);

	vk::MemoryRequirements stagingMemReq = device.getBufferMemoryRequirements(stagingBuffer);
	// 指定所需内存大小和选择一个主机可见的内存类型索引
	vk::MemoryAllocateInfo stagingMemAllocInfo(stagingMemReq.size, window_->hostVisibleMemoryIndex());
	vk::DeviceMemory stagingDevMemory = device.allocateMemory(stagingMemAllocInfo);
	device.bindBufferMemory(stagingBuffer, stagingDevMemory, 0);

	// 将Staging Buffer的内存映射到主机内存空间，以便直接访问。
	uint8_t* stagingBufferMemPtr = (uint8_t*)device.mapMemory(stagingDevMemory, 0, vertexMemReq.size);
	memcpy(stagingBufferMemPtr, vertexData, sizeof(vertexData));
	device.unmapMemory(stagingDevMemory);

	vk::CommandBufferAllocateInfo cmdBufferAlllocInfo;
	cmdBufferAlllocInfo.level = vk::CommandBufferLevel::ePrimary;
	cmdBufferAlllocInfo.commandPool = window_->graphicsCommandPool();
	cmdBufferAlllocInfo.commandBufferCount = 1;
	vk::CommandBuffer cmdBuffer = device.allocateCommandBuffers(cmdBufferAlllocInfo).front();
	vk::CommandBufferBeginInfo cmdBufferBeginInfo;	
	cmdBufferBeginInfo.flags = vk::CommandBufferUsageFlagBits::eOneTimeSubmit;	// 设置为一次性提交。
	cmdBuffer.begin(cmdBufferBeginInfo);
	vk::BufferCopy copyRegion;		// 定义缓冲区拷贝区域，源缓冲区起始偏移、目标缓冲区起始偏移及拷贝大小。
	copyRegion.srcOffset = 0;
	copyRegion.dstOffset = 0;
	copyRegion.size = stagingBufferInfo.size;
	cmdBuffer.copyBuffer(stagingBuffer, vertexBuffer_, copyRegion);
	cmdBuffer.end();

	vk::Queue graphicsQueue = window_->graphicsQueue();
	vk::SubmitInfo submitInfo;
	submitInfo.commandBufferCount = 1;	// 包含一个待提交的命令缓冲区。
	submitInfo.pCommandBuffers = &cmdBuffer;
	graphicsQueue.submit(submitInfo);
	graphicsQueue.waitIdle();			// 等待图形队列空闲，确保数据传输完成。

	device.freeCommandBuffers(window_->graphicsCommandPool(), cmdBuffer);
	device.freeMemory(stagingDevMemory);
	device.destroyBuffer(stagingBuffer);

	// ------------------------------------------------------------------------------------------------------

	vk::GraphicsPipelineCreateInfo piplineInfo;
	piplineInfo.stageCount = 2;

	auto vertShaderCode = readFile("../Hello Triangle/vert.spv");
	auto fragShaderCode = readFile("../Hello Triangle/frag.spv");

	vk::ShaderModuleCreateInfo shaderInfo;
	shaderInfo.codeSize = vertShaderCode.size();
	shaderInfo.pCode = reinterpret_cast<uint32_t*>(vertShaderCode.data());
	vk::ShaderModule vertShader = device.createShaderModule(shaderInfo);

	shaderInfo.codeSize = fragShaderCode.size();
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

	vk::VertexInputBindingDescription vertexBindingDesc;
	vertexBindingDesc.binding = 0;
	vertexBindingDesc.stride = 5 * sizeof(float);
	vertexBindingDesc.inputRate = vk::VertexInputRate::eVertex;

	vk::VertexInputAttributeDescription vertexAttrDesc[2];
	vertexAttrDesc[0].binding = 0;
	vertexAttrDesc[0].location = 0;
	vertexAttrDesc[0].format = vk::Format::eR32G32Sfloat;
	vertexAttrDesc[0].offset = 0;

	vertexAttrDesc[1].binding = 0;
	vertexAttrDesc[1].location = 1;
	vertexAttrDesc[1].format = vk::Format::eR32G32B32Sfloat;
	vertexAttrDesc[1].offset = 2 * sizeof(float);

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
	rasterizationState.frontFace = vk::FrontFace::eCounterClockwise;
	rasterizationState.lineWidth = 1.0f;
	piplineInfo.pRasterizationState = &rasterizationState;

	vk::PipelineMultisampleStateCreateInfo MSState;
	MSState.rasterizationSamples = (vk::SampleCountFlagBits)window_->sampleCountFlagBits();
	piplineInfo.pMultisampleState = &MSState;

	vk::PipelineDepthStencilStateCreateInfo DSState;
	DSState.depthTestEnable = true;
	DSState.depthWriteEnable = true;
	DSState.depthCompareOp = vk::CompareOp::eLessOrEqual;
	piplineInfo.pDepthStencilState = &DSState;

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

void TriangleRenderer::releaseResources()
{
	vk::Device device = window_->device();
	device.destroyPipeline(pipline_);
	device.destroyPipelineCache(piplineCache_);
	device.destroyPipelineLayout(piplineLayout_);
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

	cmdBuffer.bindVertexBuffers(0, vertexBuffer_, { 0 });

	cmdBuffer.draw(3, 1, 0, 0);

	cmdBuffer.endRenderPass();

	window_->frameReady();
	window_->requestUpdate();
}