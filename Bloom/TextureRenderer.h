#pragma once

#include "QVKWindow.h"

class TextureRenderer : public QVkRenderer
{
public:
	TextureRenderer();
	void initResources() override;
	void releaseResources() override;
	void startNextFrame() override;
	void updateImage(vk::ImageView image);

protected:
	vk::Sampler sampler_;

	vk::DescriptorPool descPool_;
	vk::DescriptorSetLayout descSetLayout_;
	vk::DescriptorSet descSet_;

	vk::PipelineCache piplineCache_;
	vk::PipelineLayout piplineLayout_;
	vk::Pipeline pipline_;
};

