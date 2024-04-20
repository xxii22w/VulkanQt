#pragma once
#include <qvulkanwindow.h>
#include <QVulkanWindowRenderer>
#include <vulkan/vulkan.hpp>

class TriangleRenderer : public QVulkanWindowRenderer
{
public:
	TriangleRenderer(QVulkanWindow* window);
	void initResources() override;
	void initSwapChainResources() override;
	void releaseSwapChainResources() override;
	void releaseResources() override;
	void startNextFrame() override;
private:
	QVulkanWindow* window_ = nullptr;

	vk::Buffer vertexBuffer_;
	vk::DeviceMemory vertexDevMemory_;

	vk::PipelineCache piplineCache_;
	vk::PipelineLayout piplineLayout_;
	vk::Pipeline pipline_;
};

