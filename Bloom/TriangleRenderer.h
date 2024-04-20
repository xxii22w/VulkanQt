#pragma once

#include "QVKWindow.h"

class TriangleRenderer : public QVkRenderer
{
public:
	void initResources() override;
	void releaseResources() override;
	void startNextFrame() override;
private:
	vk::Buffer vertexBuffer_;
	vk::DeviceMemory vertexDevMemory_;
	vk::PipelineCache piplineCache_;
	vk::PipelineLayout piplineLayout_;
	vk::Pipeline pipline_;
};

