#pragma once

#include <DirectXMath.h>

namespace Vibe {

    class VibeCamera {
    public:
        VibeCamera();
        ~VibeCamera();

        // 此时相机的坐标系位置
        DirectX::XMVECTOR GetPosition() const;
        DirectX::XMFLOAT3 GetPosition3f() const;
        void SetPosition(float x, float y, float z);
        void SetPosition(const DirectX::XMFLOAT3& v);

        // 此时相机的基向量
        DirectX::XMVECTOR GetRight() const;
        DirectX::XMFLOAT3 GetRight3f() const;
        DirectX::XMVECTOR GetUp() const;
        DirectX::XMFLOAT3 GetUp3f() const;
        DirectX::XMVECTOR GetLook() const;
        DirectX::XMFLOAT3 GetLook3f() const;

        // 获取视锥体属性
        float GetFovY() const;
        float GetAspect() const;
        float GetNearZ() const;
        float GetFarZ() const;

        // 设置视锥体 (透视投影)
        void SetLens(float fovY, float aspect, float zn, float zf);

        // 获取变换矩阵
        DirectX::XMMATRIX GetView() const;
        DirectX::XMMATRIX GetProj() const;
        DirectX::XMMATRIX GetViewProj() const;

        // 移动/旋转操作
        void Strafe(float d); // 左右平移
        void Walk(float d);   // 前后移动
        
        void Pitch(float angle); // 上下点头
        void RotateY(float angle); // 左右转头

        // 每一帧更新 View 矩阵
        void UpdateViewMatrix();

    private:
        // 摄像机坐标系 (在世界空间中)
        DirectX::XMFLOAT3 mPosition = { 0.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 mRight = { 1.0f, 0.0f, 0.0f };
        DirectX::XMFLOAT3 mUp = { 0.0f, 1.0f, 0.0f };
        DirectX::XMFLOAT3 mLook = { 0.0f, 0.0f, 1.0f };

        // 视锥体参数
        float mNearZ = 0.0f;
        float mFarZ = 0.0f;
        float mAspect = 0.0f;
        float mFovY = 0.0f;
        float mNearWindowHeight = 0.0f;
        float mFarWindowHeight = 0.0f;

        bool mViewDirty = true;

        // 缓存的矩阵
        DirectX::XMFLOAT4X4 mView = {};
        DirectX::XMFLOAT4X4 mProj = {};
    };

}
