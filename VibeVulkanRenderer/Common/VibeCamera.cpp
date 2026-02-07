#include "VibeCamera.h"

using namespace DirectX;

namespace Vibe {

    VibeCamera::VibeCamera() {
        SetLens(0.25f * XM_PI, 1.0f, 1.0f, 1000.0f);
    }

    VibeCamera::~VibeCamera() {
    }

    XMVECTOR VibeCamera::GetPosition() const {
        return XMLoadFloat3(&mPosition);
    }

    XMFLOAT3 VibeCamera::GetPosition3f() const {
        return mPosition;
    }

    void VibeCamera::SetPosition(float x, float y, float z) {
        mPosition = XMFLOAT3(x, y, z);
        mViewDirty = true;
    }

    void VibeCamera::SetPosition(const XMFLOAT3& v) {
        mPosition = v;
        mViewDirty = true;
    }

    XMVECTOR VibeCamera::GetRight() const {
        return XMLoadFloat3(&mRight);
    }

    XMFLOAT3 VibeCamera::GetRight3f() const {
        return mRight;
    }

    XMVECTOR VibeCamera::GetUp() const {
        return XMLoadFloat3(&mUp);
    }

    XMFLOAT3 VibeCamera::GetUp3f() const {
        return mUp;
    }

    XMVECTOR VibeCamera::GetLook() const {
        return XMLoadFloat3(&mLook);
    }

    XMFLOAT3 VibeCamera::GetLook3f() const {
        return mLook;
    }

    float VibeCamera::GetFovY() const { return mFovY; }
    float VibeCamera::GetAspect() const { return mAspect; }
    float VibeCamera::GetNearZ() const { return mNearZ; }
    float VibeCamera::GetFarZ() const { return mFarZ; }

    void VibeCamera::SetLens(float fovY, float aspect, float zn, float zf) {
        mFovY = fovY;
        mAspect = aspect;
        mNearZ = zn;
        mFarZ = zf;

        mNearWindowHeight = 2.0f * mNearZ * tanf(0.5f * mFovY);
        mFarWindowHeight = 2.0f * mFarZ * tanf(0.5f * mFovY);

        XMMATRIX P = XMMatrixPerspectiveFovLH(mFovY, mAspect, mNearZ, mFarZ);
        XMStoreFloat4x4(&mProj, P);
    }

    XMMATRIX VibeCamera::GetView() const {
        return XMLoadFloat4x4(&mView);
    }

    XMMATRIX VibeCamera::GetProj() const {
        return XMLoadFloat4x4(&mProj);
    }

    XMMATRIX VibeCamera::GetViewProj() const {
        return XMLoadFloat4x4(&mView) * XMLoadFloat4x4(&mProj);
    }

    void VibeCamera::Strafe(float d) {
        XMVECTOR s = XMVectorReplicate(d);
        XMVECTOR r = XMLoadFloat3(&mRight);
        XMVECTOR p = XMLoadFloat3(&mPosition);
        XMStoreFloat3(&mPosition, XMVectorMultiplyAdd(s, r, p));
        mViewDirty = true;
    }

    void VibeCamera::Walk(float d) {
        XMVECTOR s = XMVectorReplicate(d);
        XMVECTOR l = XMLoadFloat3(&mLook);
        XMVECTOR p = XMLoadFloat3(&mPosition);
        XMStoreFloat3(&mPosition, XMVectorMultiplyAdd(s, l, p));
        mViewDirty = true;
    }

    void VibeCamera::Pitch(float angle) {
        XMMATRIX R = XMMatrixRotationAxis(XMLoadFloat3(&mRight), angle);
        
        XMStoreFloat3(&mUp, XMVector3TransformNormal(XMLoadFloat3(&mUp), R));
        XMStoreFloat3(&mLook, XMVector3TransformNormal(XMLoadFloat3(&mLook), R));
        
        mViewDirty = true;
    }

    void VibeCamera::RotateY(float angle) {
        XMMATRIX R = XMMatrixRotationY(angle);

        XMStoreFloat3(&mRight, XMVector3TransformNormal(XMLoadFloat3(&mRight), R));
        XMStoreFloat3(&mUp, XMVector3TransformNormal(XMLoadFloat3(&mUp), R));
        XMStoreFloat3(&mLook, XMVector3TransformNormal(XMLoadFloat3(&mLook), R));

        mViewDirty = true;
    }

    void VibeCamera::UpdateViewMatrix() {
        if (!mViewDirty) return;

        XMVECTOR R = XMLoadFloat3(&mRight);
        XMVECTOR U = XMLoadFloat3(&mUp);
        XMVECTOR L = XMLoadFloat3(&mLook);
        XMVECTOR P = XMLoadFloat3(&mPosition);

        // 重新正交化，避免累积误差
        L = XMVector3Normalize(L);
        U = XMVector3Normalize(XMVector3Cross(L, R));
        R = XMVector3Cross(U, L); // U 和 L 已经正交，叉乘得到 R

        XMStoreFloat3(&mRight, R);
        XMStoreFloat3(&mUp, U);
        XMStoreFloat3(&mLook, L);

        // 构建 View 矩阵
        // Row 1
        float x = -XMVectorGetX(XMVector3Dot(P, R));
        float y = -XMVectorGetX(XMVector3Dot(P, U));
        float z = -XMVectorGetX(XMVector3Dot(P, L));

        mView(0, 0) = mRight.x; mView(0, 1) = mUp.x; mView(0, 2) = mLook.x; mView(0, 3) = 0.0f;
        mView(1, 0) = mRight.y; mView(1, 1) = mUp.y; mView(1, 2) = mLook.y; mView(1, 3) = 0.0f;
        mView(2, 0) = mRight.z; mView(2, 1) = mUp.z; mView(2, 2) = mLook.z; mView(2, 3) = 0.0f;
        mView(3, 0) = x;        mView(3, 1) = y;     mView(3, 2) = z;       mView(3, 3) = 1.0f;

        mViewDirty = false;
    }
}
