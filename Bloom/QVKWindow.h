#pragma once

#include <qvulkanwindow.h>
#include <vulkan/vulkan.hpp>
#include "QFPSCamera.h"

class QVkWindow;

class QVkRenderer : public QVulkanWindowRenderer {
	friend class QVkScene;
public:
	QVkWindow* getWindow() const { return window_; }
	void setWindow(QVkWindow* val) { window_ = val; }
protected:
	QVkWindow* window_ = nullptr;
};

//class QVkPrimitive : public QVkRenderer {
//public:
//private:
//	unsigned int id;
//	QVector3D position_;
//	QVector3D rotation_;
//	QVector3D scale_;
//};
//
//class QVkEffect : public QVkRenderer {
//private:
//	QRect geomtry_;
//};

class QVkScene : public QVulkanWindowRenderer {
public:
	QVkScene(QVkWindow* window) :window_(window) {}
	void addRenderer(std::shared_ptr<QVkRenderer> renderer);
	void removeRenderer(std::shared_ptr<QVkRenderer> renderer);
	void initResources() override;
	void releaseResources() override;
	void initSwapChainResources() override;
	void releaseSwapChainResources() override;
	void startNextFrame() override;
protected:
	QVkWindow* window_ = nullptr;
	std::list<std::shared_ptr<QVkRenderer>> rendererList_;
};

class QVkWindow : public QVulkanWindow {
public:
	QVkWindow() {
		camera_.setup(this);
	}
	void addRenderer(std::shared_ptr<QVkRenderer> renderer) {
		rendererGroup_->addRenderer(renderer);
	}
	void removeRenderer(std::shared_ptr<QVkRenderer> renderer) {
		rendererGroup_->removeRenderer(renderer);
	}
	QFpsCamera camera_;
private:
	QVulkanWindowRenderer* createRenderer() override {
		return rendererGroup_;
	}
	QVkScene* rendererGroup_ = new QVkScene(this);
};
