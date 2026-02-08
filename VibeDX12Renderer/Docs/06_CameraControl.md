# Lesson 06: Camera Control (摄像机控制)

## 1. Introduction (引言)
在前几节课中，我们看到的画面都是静止的，就像看一张挂在墙上的画。
但 3D 程序最大的魅力在于**交互**。我们希望能像在 FPS 游戏中一样，走进这个世界，从不同角度观察物体。

本节课的目标是实现一个 **First Person Camera (第一人称摄像机)**，允许用户通过键盘 (WASD) 移动，通过鼠标旋转视角。

## 2. Core Concepts (核心概念)

### 2.1 The Camera Metaphor (摄像机隐喻)
在图形学中，并没有真正的“摄像机”对象。这里所谓的“移动摄像机”，本质上是**移动整个世界**，但方向相反。
*   如果你想向**前**走 10 米，其实是把世界向**后**推 10 米。
*   如果你想向**右**转，其实是把世界向**左**旋转。

这个变换矩阵称为 **View Matrix (视图矩阵)**。它把物体从 **World Space (世界空间)** 转换到 **View Space (观察空间)**。

### 2.2 Basis Vectors (基向量)
为了描述摄像机，我们需要维护三个互相垂直的向量（UVN 系统）：
1.  **Right ($U$)**: 摄像机的右边指向哪里？
2.  **Up ($V$)**: 摄像机的头顶指向哪里？
3.  **Look ($N$)**: 摄像机的镜头指向哪里？

以及一个 **Position ($P$)** 向量。
当我们“移动”或“旋转”时，其实就是在更新这四个向量，然后每帧重新构建 View 矩阵。

### 2.3 Input Model (输入模型)
*   **键盘 (WASD)**: 控制 Position 的变化（前后左右平移）。
*   **鼠标 (Delta)**: 控制 Look 和 Right 向量的方向（俯仰 Pitch 和 偏航 Yaw）。

---

## 3. Code Implementation (代码实现)

### 3.1 The Camera Class (封装摄像机)
为了代码整洁，我们将摄像机逻辑封装在 `VibeCamera` 类中。

```cpp
class VibeCamera {
public:
    // ... Getters/Setters ...

    // 核心操作：构建视图矩阵
    void UpdateViewMatrix();

    // 移动操作
    void Walk(float d);  // 前后移动 (沿 Look 向量)
    void Strafe(float d); // 左右横移 (沿 Right 向量)

    // 旋转操作
    void Pitch(float angle); // 抬头/低头 (绕 Right 向量转)
    void RotateY(float angle); // 左右转头 (绕世界 Y 轴转)

private:
    XMFLOAT3 mPosition = { 0.0f, 0.0f, 0.0f };
    XMFLOAT3 mRight = { 1.0f, 0.0f, 0.0f };
    XMFLOAT3 mUp = { 0.0f, 1.0f, 0.0f };
    XMFLOAT3 mLook = { 0.0f, 0.0f, 1.0f };
    
    // 最终生成的 View 矩阵
    XMFLOAT4X4 mView = MathHelper::Identity4x4();
};
```

### 3.2 Updating Vectors (更新向量)
这是最数学的部分。
**Walk (移动)** 比较简单，只是位置的加减。
**Pitch (旋转)** 则需要用到矩阵变换。当我们抬头时，$Look$ 向量和 $Up$ 向量都会发生改变，但 $Right$ 向量不变。

```cpp
void VibeCamera::Pitch(float angle) {
    // 1. 构建一个绕 Right 轴旋转的矩阵
    XMMATRIX R = XMMatrixRotationAxis(XMLoadFloat3(&mRight), angle);

    // 2. 用这个矩阵变换 Look 和 Up 向量
    XMStoreFloat3(&mUp, XMVector3TransformNormal(XMLoadFloat3(&mUp), R));
    XMStoreFloat3(&mLook, XMVector3TransformNormal(XMLoadFloat3(&mLook), R));
}

void VibeCamera::UpdateViewMatrix() {
    XMVECTOR R = XMLoadFloat3(&mRight);
    XMVECTOR U = XMLoadFloat3(&mUp);
    XMVECTOR L = XMLoadFloat3(&mLook);
    XMVECTOR P = XMLoadFloat3(&mPosition);

    // 1. 向量正交化 (Renormalize)
    // 经过多次旋转计算后，浮点误差会让这三个向量不再垂直。
    // 我们必须强制让它们重新垂直并归一化。
    L = XMVector3Normalize(L);
    U = XMVector3Normalize(XMVector3Cross(L, R)); // Up = Look x Right
    R = XMVector3Cross(U, L); // Right = Up x Look

    // 2. 填充 View 矩阵
    // View = R_transposed * T_negative
    // 这里使用 DirectXMath 的辅助函数 LookTo 来简化
    // 但理解原理很重要：Row1=(Rx, Ry, Rz), Row2=(Ux, Uy, Uz)...
    // ...
}
```

### 3.3 Handling Input (处理输入)
我们需要在 `MsgProc` 中拦截鼠标消息，并在 `OnUpdate` 中检测键盘状态。

**鼠标捕捉 (Mouse Capture)**:
当按下鼠标左键时，我们“捕捉”鼠标，这样即使鼠标移出窗口，我们也依然能收到消息。这对于拖拽操作很重要。

```cpp
LRESULT CameraControlApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch(msg) {
        case WM_LBUTTONDOWN:
            // 记录当前位置，并隐藏/捕捉鼠标
            mLastMousePos.x = LOWORD(lParam);
            mLastMousePos.y = HIWORD(lParam);
            SetCapture(hwnd); 
            return 0;
            
        case WM_MOUSEMOVE:
            if (wParam & MK_LBUTTON) { // 只有按住左键时才旋转
                // 计算差值
                float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
                float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));

                // 应用到摄像机
                m_camera.Pitch(dy);
                m_camera.RotateY(dx);
            }
            // 更新“上一次”位置
            mLastMousePos.x = x;
            mLastMousePos.y = y;
            return 0;
            
        case WM_LBUTTONUP:
            ReleaseCapture();
            return 0;
    }
    return VibeDX12App::MsgProc(...);
}
```

**键盘平滑移动**:
不要在 `WM_KEYDOWN` 里处理移动，这会有延迟和卡顿。
最佳实践是在 `OnUpdate` 每一帧去查询键位状态。

```cpp
void CameraControlApp::OnUpdate() {
    float speed = 10.0f * dt; // 移动速度 * 时间步长

    // GetAsyncKeyState: 实时查询物理按键状态
    // 0x8000 表示按键当前被按下
    if (GetAsyncKeyState('W') & 0x8000) m_camera.Walk(speed);
    if (GetAsyncKeyState('S') & 0x8000) m_camera.Walk(-speed);
    if (GetAsyncKeyState('A') & 0x8000) m_camera.Strafe(-speed);
    if (GetAsyncKeyState('D') & 0x8000) m_camera.Strafe(speed);

    // ！！！ 更新 View 矩阵 ！！！
    m_camera.UpdateViewMatrix();
}
```

### 3.4 Passing Matrices to Shader (传递矩阵给 Shader)
现在 View 矩阵是动态的了，我们每帧都需要把它传给 GPU。

```cpp
void CameraControlApp::OnRender() {
    // ...
    
    // 1. 获取最新的 View 和 Proj 矩阵
    XMMATRIX view = m_camera.GetView();
    XMMATRIX proj = m_camera.GetProj();
    XMMATRIX world = XMMatrixIdentity(); // 物体不一定要动

    // 2. 组合 MVP
    XMMATRIX worldViewProj = world * view * proj;

    // 3. 上传到 Root Constants
    m_commandList->SetGraphicsRoot32BitConstants(0, 16, &worldViewProj, 0);

    // ... Draw ...
}
```

## 4. Summary (总结)
通过引入 `VibeCamera` 类和输入处理，我们将观察者的视角从固定的点解放了出来。
虽然背后的数学（向量叉乘、矩阵变换）略显枯燥，但这是所有 3D 游戏漫游功能的基石。

现在的你，已经可以在自己构建的 3D 世界中自由行走了！

