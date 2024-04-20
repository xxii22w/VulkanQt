#include "OffscreenRenderer.h"
#include <QDesktopServices>
#include <QUrl>

OffscreenRenderer::OffscreenRenderer(QVulkanWindow* window)
	:window_(window) {
}

void OffscreenRenderer::initResources() {
	QImage image = createImage({ 800,600 }, QColor(0, 100, 200));
	image.save("output.png");
	QDesktopServices::openUrl(QUrl("file:", QUrl::TolerantMode));	// QUrl::TolerantMode表示在解析过程中尽可能地接受并纠正不规范的URL格式。
}

void OffscreenRenderer::initSwapChainResources() {
}

void OffscreenRenderer::releaseSwapChainResources()
{
}

void OffscreenRenderer::releaseResources() {
}

void OffscreenRenderer::startNextFrame() {
}

QImage OffscreenRenderer::createImage(QSize size, QColor color)
{
	vk::Device device = window_->device();

	vk::AttachmentDescription attachmentDesc;
	attachmentDesc.format = vk::Format::eR8G8B8A8Unorm;
	attachmentDesc.samples = vk::SampleCountFlagBits::e1;
	attachmentDesc.loadOp = vk::AttachmentLoadOp::eClear;
	attachmentDesc.storeOp = vk::AttachmentStoreOp::eStore;
	attachmentDesc.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
	attachmentDesc.stencilStoreOp = vk::AttachmentStoreOp::eDontCare;
	attachmentDesc.initialLayout = vk::ImageLayout::eUndefined;
	attachmentDesc.finalLayout = vk::ImageLayout::eTransferSrcOptimal;	// 布局为转储源优化

	// 指定颜色附件在子通道中的布局和索引。
	vk::AttachmentReference attachmentRef;
	attachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;
	attachmentRef.attachment = 0;

	// 建了一个图形管道绑定点的子通道，包含一个颜色附件引用
	vk::SubpassDescription subpassDesc;
	subpassDesc.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
	subpassDesc.colorAttachmentCount = 1;
	subpassDesc.pColorAttachments = &attachmentRef;

	// 创建渲染通道创建信息 
	vk::RenderPassCreateInfo renderPassInfo;
	renderPassInfo.attachmentCount = 1;
	renderPassInfo.pAttachments = &attachmentDesc;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpassDesc;
	// 创建渲染通道
	vk::RenderPass offscreenRenderPass = device.createRenderPass(renderPassInfo);

	// 创建颜色附件图像
	vk::ImageCreateInfo attachmentImageInfo;
	attachmentImageInfo.imageType = vk::ImageType::e2D;
	attachmentImageInfo.format = vk::Format::eR8G8B8A8Unorm;
	attachmentImageInfo.extent.width = size.width();
	attachmentImageInfo.extent.height = size.height();
	attachmentImageInfo.extent.depth = 1;
	attachmentImageInfo.mipLevels = 1;
	attachmentImageInfo.arrayLayers = 1;
	attachmentImageInfo.samples = vk::SampleCountFlagBits::e1;
	attachmentImageInfo.tiling = vk::ImageTiling::eOptimal;
	attachmentImageInfo.usage = vk::ImageUsageFlagBits::eColorAttachment | vk::ImageUsageFlagBits::eTransferSrc;
	vk::Image colorAttachment = device.createImage(attachmentImageInfo);

	// 分配和绑定图像内存
	vk::MemoryRequirements memReq = device.getImageMemoryRequirements(colorAttachment);
	vk::MemoryAllocateInfo memAllocInfo(memReq.size, window_->deviceLocalMemoryIndex());
	// 创建内存分配信息，分配设备内存并将其绑定到颜色附件图像。
	vk::DeviceMemory attachmentImageMemory = device.allocateMemory(memAllocInfo);
	device.bindImageMemory(colorAttachment, attachmentImageMemory, 0);

	vk::ImageViewCreateInfo imageViewInfo;
	imageViewInfo.viewType = vk::ImageViewType::e2D;
	imageViewInfo.format = attachmentImageInfo.format;
	imageViewInfo.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
	imageViewInfo.subresourceRange.levelCount = imageViewInfo.subresourceRange.layerCount = 1;
	imageViewInfo.components.r = vk::ComponentSwizzle::eR;
	imageViewInfo.components.g = vk::ComponentSwizzle::eG;
	imageViewInfo.components.b = vk::ComponentSwizzle::eB;
	imageViewInfo.components.a = vk::ComponentSwizzle::eA;
	imageViewInfo.image = colorAttachment;
	vk::ImageView imageView = device.createImageView(imageViewInfo);

	// 使用离屏渲染通道、颜色附件视图及尺寸信息创建帧缓冲
	vk::FramebufferCreateInfo framebufferInfo;
	framebufferInfo.renderPass = offscreenRenderPass;
	framebufferInfo.attachmentCount = 1;
	framebufferInfo.pAttachments = &imageView;
	framebufferInfo.width = size.width();
	framebufferInfo.height = size.height();
	framebufferInfo.layers = 1;
	vk::Framebuffer frameBuffer = device.createFramebuffer(framebufferInfo);

	vk::CommandBufferAllocateInfo cmdBufferAlllocInfo;
	cmdBufferAlllocInfo.level = vk::CommandBufferLevel::ePrimary;
	cmdBufferAlllocInfo.commandPool = window_->graphicsCommandPool();
	cmdBufferAlllocInfo.commandBufferCount = 1;
	vk::CommandBuffer cmdBuffer = device.allocateCommandBuffers(cmdBufferAlllocInfo).front();
	vk::CommandBufferBeginInfo cmdBufferBeginInfo;
	cmdBufferBeginInfo.flags = vk::CommandBufferUsageFlagBits::eRenderPassContinue;
	cmdBuffer.begin(cmdBufferBeginInfo);

	vk::ClearValue clearValues = vk::ClearColorValue(std::array<float, 4>{color.redF(), color.greenF(), color.blueF(), color.alphaF() });
	// 创建渲染通道开始信息（包括渲染通道、帧缓冲、渲染区域、清除值数量和指针）
	vk::RenderPassBeginInfo beginInfo;
	beginInfo.renderPass = offscreenRenderPass;
	beginInfo.framebuffer = frameBuffer;
	beginInfo.renderArea.extent.width = size.width();
	beginInfo.renderArea.extent.height = size.height();
	beginInfo.clearValueCount = 1;
	beginInfo.pClearValues = &clearValues;

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
	cmdBuffer.endRenderPass();

	// 创建临时（转储）图像 (vk::Image): 创建一个线性铺排、用于转储目的的临时图像
	vk::ImageCreateInfo stagingImageInfo;
	stagingImageInfo.imageType = vk::ImageType::e2D;
	stagingImageInfo.format = vk::Format::eR8G8B8A8Unorm;
	stagingImageInfo.extent.width = size.width();
	stagingImageInfo.extent.height = size.height();
	stagingImageInfo.extent.depth = 1;
	stagingImageInfo.mipLevels = 1;
	stagingImageInfo.arrayLayers = 1;
	stagingImageInfo.samples = vk::SampleCountFlagBits::e1;
	stagingImageInfo.tiling = vk::ImageTiling::eLinear;
	stagingImageInfo.usage = vk::ImageUsageFlagBits::eTransferDst;
	stagingImageInfo.initialLayout = vk::ImageLayout::eUndefined;
	vk::Image stagingImage = device.createImage(stagingImageInfo);
	// 分配和绑定临时图像内存
	vk::MemoryRequirements stagingMemReq = device.getImageMemoryRequirements(stagingImage);
	vk::MemoryAllocateInfo stagingBufferAllocInfo(stagingMemReq.size, window_->hostVisibleMemoryIndex());
	vk::DeviceMemory stagingImageMemory = device.allocateMemory(stagingBufferAllocInfo);
	device.bindImageMemory(stagingImage, stagingImageMemory, 0);

	vk::ImageSubresourceRange stagingSubResRange;
	stagingSubResRange.aspectMask = vk::ImageAspectFlagBits::eColor;
	stagingSubResRange.baseMipLevel = 0;
	stagingSubResRange.levelCount = 1;
	stagingSubResRange.baseArrayLayer = 0;
	stagingSubResRange.layerCount = 1;

	// 定义图像内存屏障 创建一个内存屏障，用于将临时图像从未定义布局转换为转储目标优化布局。
	vk::ImageMemoryBarrier barrier;
	barrier.image = stagingImage;
	barrier.dstAccessMask = vk::AccessFlagBits::eTransferWrite;
	barrier.oldLayout = vk::ImageLayout::eUndefined;
	barrier.newLayout = vk::ImageLayout::eTransferDstOptimal;
	barrier.subresourceRange = stagingSubResRange;
	cmdBuffer.pipelineBarrier(
		vk::PipelineStageFlagBits::eTransfer,
		vk::PipelineStageFlagBits::eTransfer,
		{},
		0, nullptr,
		0, nullptr,
		1, &barrier
	);

	// 记录图像复制命令 (vk::ImageCopy): 定义从颜色附件到临时图像的图像复制操作。
	vk::ImageCopy imageCopy;
	imageCopy.srcSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
	imageCopy.srcSubresource.layerCount = 1;
	imageCopy.dstSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
	imageCopy.dstSubresource.layerCount = 1;
	imageCopy.extent.width = size.width();
	imageCopy.extent.height = size.height();
	imageCopy.extent.depth = 1;

	cmdBuffer.copyImage(colorAttachment, vk::ImageLayout::eTransferSrcOptimal, stagingImage, vk::ImageLayout::eTransferDstOptimal, imageCopy);

	cmdBuffer.end();

	vk::Queue graphicsQueue = window_->graphicsQueue();
	vk::SubmitInfo submitInfo;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &cmdBuffer;
	graphicsQueue.submit(submitInfo);
	graphicsQueue.waitIdle();
	device.freeCommandBuffers(window_->graphicsCommandPool(), cmdBuffer);

	// 映射临时图像内存: 获取临时图像的子资源布局，映射其内存到主机可访问的指针（memPtr）。
	vk::ImageSubresource stagingSubRes;
	stagingSubRes.aspectMask = vk::ImageAspectFlagBits::eColor;
	stagingSubRes.arrayLayer = 0;
	stagingSubRes.mipLevel = 0;
	vk::SubresourceLayout layout = device.getImageSubresourceLayout(stagingImage, stagingSubRes);
	uint8_t* memPtr = (uint8_t*)device.mapMemory(stagingImageMemory, layout.offset, layout.size, {});

	QImage readBackImage(size, QImage::Format::Format_RGBA8888);
	if (memPtr) {
		for (int y = 0; y < readBackImage.height(); ++y) {
			memcpy(readBackImage.scanLine(y), memPtr, readBackImage.width() * 4);
			memPtr += layout.rowPitch;
		}
		device.unmapMemory(stagingImageMemory);
	}
	else {
		qWarning("QVulkanWindow: Failed to map readback image memory after transfer");
	}

	device.destroyImage(stagingImage);
	device.freeMemory(stagingImageMemory);
	device.destroyImageView(imageView);
	device.destroyImage(colorAttachment);
	device.freeMemory(attachmentImageMemory);
	device.destroyFramebuffer(frameBuffer);
	device.destroyRenderPass(offscreenRenderPass);

	return readBackImage;
}