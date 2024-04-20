#ifndef SkeletonAnimation_h__
#define SkeletonAnimation_h__

#include <map>
#include <memory>
#include "assimp/anim.h"
#include "SkeletonMeshNode.h"

class SkeletonAnimation {
public:
	SkeletonAnimation(aiAnimation* animation);
	void createAnimationMatrixInternal(std::shared_ptr<SkeletonBoneNode> boneNode, std::vector<aiMatrix4x4>& matrixs, float time, aiMatrix4x4 parentMatrix = aiMatrix4x4());
private:
	struct BoneKeyFrame {
		std::map<float, aiVector3D> translation;
		std::map<float, aiQuaternion> rotation;
		std::map<float, aiVector3D> scaling;
		aiMatrix4x4 createMatrixByTimeMs(const float& timeMs);
	};

	std::map<std::string, BoneKeyFrame> boneAnimationNode_;
	float duration_;			// 期间_
	float ticksPerSecond_;		// 每Second_的刻度
};

#endif // SkeletonAnimation_h__