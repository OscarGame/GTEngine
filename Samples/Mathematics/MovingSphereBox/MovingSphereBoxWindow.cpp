#include "MovingSphereBoxWindow.h"

int main(int, char const*[])
{
#if defined(_DEBUG)
    LogReporter reporter(
        "LogReport.txt",
        Logger::Listener::LISTEN_FOR_ALL,
        Logger::Listener::LISTEN_FOR_ALL,
        Logger::Listener::LISTEN_FOR_ALL,
        Logger::Listener::LISTEN_FOR_ALL);
#endif

    Window::Parameters parameters(L"MovingSphereBoxWindow", 0, 0, 768, 768);
    auto window = TheWindowSystem.Create<MovingSphereBoxWindow>(parameters);
    TheWindowSystem.MessagePump(window, TheWindowSystem.DEFAULT_ACTION);
    TheWindowSystem.Destroy<MovingSphereBoxWindow>(window);
    return 0;
}

MovingSphereBoxWindow::MovingSphereBoxWindow(Parameters& parameters)
    :
    Window3(parameters),
    mAlpha(0.5f)
{
    mBlendState = std::make_shared<BlendState>();
    mBlendState->target[0].enable = true;
    mBlendState->target[0].srcColor = BlendState::BM_SRC_ALPHA;
    mBlendState->target[0].dstColor = BlendState::BM_INV_SRC_ALPHA;
    mBlendState->target[0].srcAlpha = BlendState::BM_SRC_ALPHA;
    mBlendState->target[0].dstAlpha = BlendState::BM_INV_SRC_ALPHA;

    mNoCullState = std::make_shared<RasterizerState>();
    mNoCullState->cullMode = RasterizerState::CULL_NONE;
    mEngine->SetRasterizerState(mNoCullState);

    CreateScene();

    InitializeCamera(60.0f, GetAspectRatio(), 0.1f, 100.0f, 0.001f, 0.001f,
        { 24.0f, 0.0f, 0.0f }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f });

    mTrackball.Update();
    mPVWMatrices.Update();
}

void MovingSphereBoxWindow::OnIdle()
{
    mTimer.Measure();

    if (mCameraRig.Move())
    {
        mPVWMatrices.Update();
    }

    mEngine->ClearBuffers();

    // This is not the correct drawing order, but it is close enough for
    // demonstrating the moving sphere-box intersection query.
    mEngine->SetBlendState(mBlendState);
    mEngine->Draw(mBoxVisual);
    for (int i = 0; i < 8; ++i)
    {
        mEngine->Draw(mVertexVisual[i]);
    }
    for (int i = 0; i < 12; ++i)
    {
        mEngine->Draw(mEdgeVisual[i]);
    }
    for (int i = 0; i < 6; ++i)
    {
        mEngine->Draw(mFaceVisual[i]);
    }
    mEngine->SetDefaultBlendState();

    mEngine->Draw(8, mYSize - 8, { 0.0f, 0.0f, 0.0f, 1.0f }, mTimer.GetFPS());
    mEngine->DisplayColorBuffer(0);

    mTimer.UpdateFrameCount();
}

bool MovingSphereBoxWindow::OnCharPress(unsigned char key, int x, int y)
{
    switch (key)
    {
    case 'w':
    case 'W':
        if (mNoCullState == mEngine->GetRasterizerState())
        {
            mEngine->SetDefaultRasterizerState();
        }
        else
        {
            mEngine->SetRasterizerState(mNoCullState);
        }
        return true;
    }
    return Window3::OnCharPress(key, x, y);
}

void MovingSphereBoxWindow::CreateScene()
{
    CreateRoundedBoxVertices();
    CreateRoundedBoxEdges();
    CreateRoundedBoxFaces();
    CreateBox();
}

void MovingSphereBoxWindow::CreateRoundedBoxVertices()
{
    mSphere.center = { 0.0f, 0.0f, 0.0f };
    mSphere.radius = 1.0f;

    float sqrt2 = sqrt(2.0f), sqrt3 = sqrt(3.0f);
    float a0 = (sqrt3 - 1.0f) / sqrt3;
    float a1 = (sqrt3 + 1.0f) / (2.0f * sqrt3);
    float a2 = 1.0f - (5.0f - sqrt2) * (7.0f - sqrt3) / 46.0f;
    float b0 = 4.0f * sqrt3 * (sqrt3 - 1.0f);
    float b1 = 3.0f * sqrt2;
    float b2 = 4.0f;
    float b3 = sqrt2 * (3.0f + 2.0f * sqrt2 - sqrt3) / sqrt3;

    Vector3<float> control[5][5];
    float weight[5][5];
    std::function<float(float, float, float)> bernstein[5][5];

    control[0][0] = { 0.0f, 0.0f, 1.0f };  // P004
    control[0][1] = { 0.0f, a0,   1.0f };  // P013
    control[0][2] = { 0.0f, a1,   a1 };  // P022
    control[0][3] = { 0.0f, 1.0f, a0 };  // P031
    control[0][4] = { 0.0f, 1.0f, 0.0f };  // P040

    control[1][0] = { a0,   0.0f, 1.0f };  // P103
    control[1][1] = { a2,   a2,   1.0f };  // P112
    control[1][2] = { a2,   1.0f, a2 };  // P121
    control[1][3] = { a0,   1.0f, 0.0f };  // P130
    control[1][4] = { 0.0f, 0.0f, 0.0f };  // unused

    control[2][0] = { a1,   0.0f, a1 };  // P202
    control[2][1] = { 1.0f, a2,   a2 };  // P211
    control[2][2] = { a1,   a1,   0.0f };  // P220
    control[2][3] = { 0.0f, 0.0f, 0.0f };  // unused
    control[2][4] = { 0.0f, 0.0f, 0.0f };  // unused

    control[3][0] = { 1.0f, 0.0f, a0 };  // P301
    control[3][1] = { 1.0f, a0,   0.0f };  // P310
    control[3][2] = { 0.0f, 0.0f, 0.0f };  // unused
    control[3][3] = { 0.0f, 0.0f, 0.0f };  // unused
    control[3][4] = { 0.0f, 0.0f, 0.0f };  // unused

    control[4][0] = { 1.0f, 0.0f, 0.0f };  // P400
    control[4][1] = { 0.0f, 0.0f, 0.0f };  // unused
    control[4][2] = { 0.0f, 0.0f, 0.0f };  // unused
    control[4][3] = { 0.0f, 0.0f, 0.0f };  // unused
    control[4][4] = { 0.0f, 0.0f, 0.0f };  // unused

    weight[0][0] = b0;    // w004
    weight[0][1] = b1;    // w013
    weight[0][2] = b2;    // w022
    weight[0][3] = b1;    // w031
    weight[0][4] = b0;    // w040

    weight[1][0] = b1;    // w103
    weight[1][1] = b3;    // w112
    weight[1][2] = b3;    // w121
    weight[1][3] = b1;    // w130
    weight[1][4] = 0.0f;  // unused

    weight[2][0] = b2;    // w202
    weight[2][1] = b3;    // w211
    weight[2][2] = b2;    // w220
    weight[2][3] = 0.0f;  // unused
    weight[2][4] = 0.0f;  // unused

    weight[3][0] = b1;    // w301
    weight[3][1] = b1;    // w310
    weight[3][2] = 0.0f;  // unused
    weight[3][3] = 0.0f;  // unused
    weight[3][4] = 0.0f;  // unused

    weight[4][0] = b0;    // w400
    weight[4][1] = 0.0f;  // unused
    weight[4][2] = 0.0f;  // unused
    weight[4][3] = 0.0f;  // unused
    weight[4][4] = 0.0f;  // unused

    bernstein[0][0] = [](float, float, float w) { return w * w * w * w; };
    bernstein[0][1] = [](float, float v, float w) { return 4.0f * v * w * w * w; };
    bernstein[0][2] = [](float, float v, float w) { return 6.0f * v * v * w * w; };
    bernstein[0][3] = [](float, float v, float w) { return 4.0f * v * v * v * w; };
    bernstein[0][4] = [](float, float v, float) { return v * v * v * v; };
    bernstein[1][0] = [](float u, float, float w) { return 4.0f * u * w * w * w; };
    bernstein[1][1] = [](float u, float v, float w) { return 12.0f * u * v * w * w; };
    bernstein[1][2] = [](float u, float v, float w) { return 12.0f * u * v * v * w; };
    bernstein[1][3] = [](float u, float v, float) { return 4.0f * u * v * v * v; };
    bernstein[2][0] = [](float u, float, float w) { return 6.0f * u * u * w * w; };
    bernstein[2][1] = [](float u, float v, float w) { return 12.0f * u * u * v * w; };
    bernstein[2][2] = [](float u, float v, float) { return 6.0f * u * u * v * v; };
    bernstein[3][0] = [](float u, float, float w) { return 4.0f * u * u * u * w; };
    bernstein[3][1] = [](float u, float v, float) { return 4.0f * u * u * u * v; };
    bernstein[4][0] = [](float u, float, float) { return u * u * u * u; };

    VertexFormat vformat;
    vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
    auto vbuffer = std::make_shared<VertexBuffer>(vformat, DENSITY * DENSITY);
    Vector3<float>* vertices = vbuffer->Get<Vector3<float>>();
    memset(vbuffer->GetData(), 0, vbuffer->GetNumBytes());
    for (int iv = 0; iv <= DENSITY - 1; ++iv)
    {
        float v = (float)iv / (float)(DENSITY - 1);
        for (int iu = 0; iu + iv <= DENSITY - 1; ++iu)
        {
            float u = (float)iu / (float)(DENSITY - 1);
            float w = 1.0f - u - v;

            Vector3<float> numer{ 0.0f, 0.0f, 0.0f };
            float denom = 0.0f;
            for (int j1 = 0; j1 <= 4; ++j1)
            {
                for (int j0 = 0; j0 + j1 <= 4; ++j0)
                {
                    float product = weight[j1][j0] * bernstein[j1][j0](u, v, w);
                    numer += product * control[j1][j0];
                    denom += product;
                }
            }

            vertices[iu + DENSITY * iv] = mSphere.radius * numer / denom;
        }
    }

    std::vector<int> indices;
    for (int iv = 0; iv <= DENSITY - 2; ++iv)
    {
        // two triangles per square
        int iu, j0, j1, j2, j3;
        for (iu = 0; iu + iv <= DENSITY - 3; ++iu)
        {
            j0 = iu + DENSITY * iv;
            j1 = j0 + 1;
            j2 = j0 + DENSITY;
            j3 = j2 + 1;
            indices.push_back(j0);
            indices.push_back(j1);
            indices.push_back(j2);
            indices.push_back(j1);
            indices.push_back(j3);
            indices.push_back(j2);
        }

        // last triangle in row is singleton
        j0 = iu + DENSITY * iv;
        j1 = j0 + 1;
        j2 = j0 + DENSITY;
        indices.push_back(j0);
        indices.push_back(j1);
        indices.push_back(j2);
    }

    uint32_t numTriangles = (uint32_t)(indices.size() / 3);
    auto ibuffer = std::make_shared<IndexBuffer>(IP_TRIMESH, numTriangles, sizeof(int));
    memcpy(ibuffer->GetData(), indices.data(), indices.size() * sizeof(int));

    float e0 = 3.0f, e1 = 2.0f, e2 = 1.0f;
    mBox.min = { -e0, -e1, -e2 };
    mBox.max = { +e0, +e1, +e2 };

    Vector4<float> color[8] =
    {
        Vector4<float>{ 0.0f, 0.5f, 0.0f, mAlpha },
        Vector4<float>{ 0.0f, 0.5f, 0.0f, mAlpha },
        Vector4<float>{ 0.0f, 0.5f, 0.0f, mAlpha },
        Vector4<float>{ 0.0f, 0.5f, 0.0f, mAlpha },
        Vector4<float>{ 0.0f, 0.5f, 0.0f, mAlpha },
        Vector4<float>{ 0.0f, 0.5f, 0.0f, mAlpha },
        Vector4<float>{ 0.0f, 0.5f, 0.0f, mAlpha },
        Vector4<float>{ 0.0f, 1.0f, 0.0f, mAlpha }
    };

    Vector3<float> center[8] =
    {
        Vector3<float>{ -e0, -e1, -e2 },
        Vector3<float>{ +e0, -e1, -e2 },
        Vector3<float>{ -e0, +e1, -e2 },
        Vector3<float>{ +e0, +e1, -e2 },
        Vector3<float>{ -e0, -e1, +e2 },
        Vector3<float>{ +e0, -e1, +e2 },
        Vector3<float>{ -e0, +e1, +e2 },
        Vector3<float>{ +e0, +e1, +e2 }
    };

    float sqrtHalf = sqrt(0.5f);
    Quaternion<float> orient[8] =
    {
        Quaternion<float>{ sqrtHalf, 0.0f, -sqrtHalf, 0.0f },
        Quaternion<float>{ 0.5f, -0.5f, 0.5f, -0.5f },
        Quaternion<float>{ 0.0f, 1.0f, 0.0f, 0.0f },
        Quaternion<float>{ 0.0f, sqrtHalf, 0.0f, sqrtHalf },
        Quaternion<float>{ 0.0f, 0.0f, 1.0f, 0.0f },
        Quaternion<float>{ 0.0f, 0.0f, -sqrtHalf, sqrtHalf },
        Quaternion<float>{ 0.0f, 0.0f, sqrtHalf, sqrtHalf },
        Quaternion<float>{ 0.0f, 0.0f, 0.0f, 1.0f }
    };

    mVNormal[0] = { -1.0f, -1.0f, -1.0f, 0.0f };
    mVNormal[1] = { +1.0f, -1.0f, -1.0f, 0.0f };
    mVNormal[2] = { -1.0f, +1.0f, -1.0f, 0.0f };
    mVNormal[3] = { +1.0f, +1.0f, -1.0f, 0.0f };
    mVNormal[4] = { -1.0f, -1.0f, +1.0f, 0.0f };
    mVNormal[5] = { +1.0f, -1.0f, +1.0f, 0.0f };
    mVNormal[6] = { -1.0f, +1.0f, +1.0f, 0.0f };
    mVNormal[7] = { +1.0f, +1.0f, +1.0f, 0.0f };

    for (int i = 0; i < 8; ++i)
    {
        auto effect = std::make_shared<ConstantColorEffect>(mProgramFactory, color[i]);
        mVertexVisual[i] = std::make_shared<Visual>(vbuffer, ibuffer, effect);
        mVertexVisual[i]->localTransform.SetTranslation(center[i]);
        mVertexVisual[i]->localTransform.SetRotation(orient[i]);
        mPVWMatrices.Subscribe(mVertexVisual[i]->worldTransform, effect->GetPVWMatrixConstant());
        mTrackball.Attach(mVertexVisual[i]);
    }
}

void MovingSphereBoxWindow::CreateRoundedBoxEdges()
{
    VertexFormat vformat;
    vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
    MeshFactory mf;
    mf.SetVertexFormat(vformat);
    auto visual = mf.CreateRectangle(DENSITY, DENSITY, 1.0f, 1.0f);
    auto vbuffer = visual->GetVertexBuffer();
    auto ibuffer = visual->GetIndexBuffer();
    Vector3<float>* vertices = vbuffer->Get<Vector3<float>>();
    for (int row = 0; row < DENSITY; ++row)
    {
        //float z = mBox.min[2] + (mBox.max[2] - mBox.min[2]) * (float)row / (float)(DENSITY - 1);
        float z = -1.0f + 2.0f * (float)row / (float)(DENSITY - 1);
        for (int col = 0; col < DENSITY; ++col)
        {
            float angle = (float)GTE_C_HALF_PI * (float)col / (float)(DENSITY - 1);
            float cs = cos(angle), sn = sin(angle);
            *vertices++ = Vector3<float>{ mSphere.radius * cs, mSphere.radius * sn, z };
        }
    }

    Vector4<float> color[12] =
    {
        Vector4<float>{ 0.5f, 0.0f, 0.0f, mAlpha },
        Vector4<float>{ 0.5f, 0.0f, 0.0f, mAlpha },
        Vector4<float>{ 0.5f, 0.0f, 0.0f, mAlpha },
        Vector4<float>{ 0.5f, 0.0f, 0.0f, mAlpha },
        Vector4<float>{ 1.0f, 0.5f, 0.0f, mAlpha },
        Vector4<float>{ 1.0f, 0.5f, 0.0f, mAlpha },
        Vector4<float>{ 1.0f, 0.5f, 0.0f, mAlpha },
        Vector4<float>{ 1.0f, 0.5f, 0.0f, mAlpha },
        Vector4<float>{ 0.0f, 0.25f, 0.5f, mAlpha },
        Vector4<float>{ 0.0f, 0.25f, 0.5f, mAlpha },
        Vector4<float>{ 0.0f, 0.25f, 0.5f, mAlpha },
        Vector4<float>{ 0.0f, 0.25f, 0.5f, mAlpha }
    };

    Vector3<float> center[12] =
    {
        Vector3<float>{ -mBox.max[0], -mBox.max[1], 0.0f },
        Vector3<float>{ +mBox.max[0], -mBox.max[1], 0.0f },
        Vector3<float>{ -mBox.max[0], +mBox.max[1], 0.0f },
        Vector3<float>{ +mBox.max[0], +mBox.max[1], 0.0f },
        Vector3<float>{ -mBox.max[0], 0.0f, -mBox.max[2] },
        Vector3<float>{ +mBox.max[0], 0.0f, -mBox.max[2] },
        Vector3<float>{ -mBox.max[0], 0.0f, +mBox.max[2] },
        Vector3<float>{ +mBox.max[0], 0.0f, +mBox.max[2] },
        Vector3<float>{ 0.0f, -mBox.max[1], -mBox.max[2] },
        Vector3<float>{ 0.0f, +mBox.max[1], -mBox.max[2] },
        Vector3<float>{ 0.0f, -mBox.max[1], +mBox.max[2] },
        Vector3<float>{ 0.0f, +mBox.max[1], +mBox.max[2] }
    };

    Vector3<float> scale[12] =
    {
        Vector3<float>{ 1.0f, 1.0f, mBox.max[2] },
        Vector3<float>{ 1.0f, 1.0f, mBox.max[2] },
        Vector3<float>{ 1.0f, 1.0f, mBox.max[2] },
        Vector3<float>{ 1.0f, 1.0f, mBox.max[2] },
        Vector3<float>{ 1.0f, 1.0f, mBox.max[1] },
        Vector3<float>{ 1.0f, 1.0f, mBox.max[1] },
        Vector3<float>{ 1.0f, 1.0f, mBox.max[1] },
        Vector3<float>{ 1.0f, 1.0f, mBox.max[1] },
        Vector3<float>{ 1.0f, 1.0f, mBox.max[0] },
        Vector3<float>{ 1.0f, 1.0f, mBox.max[0] },
        Vector3<float>{ 1.0f, 1.0f, mBox.max[0] },
        Vector3<float>{ 1.0f, 1.0f, mBox.max[0] }
    };

    float sqrtHalf = sqrt(0.5f);
    Quaternion<float> orient[12] =
    {
        Quaternion<float>{ 0.0f, 0.0f, 1.0f, 0.0f },
        Quaternion<float>{ 0.0f, 0.0f, -sqrtHalf, sqrtHalf },
        Quaternion<float>{ 0.0f, 0.0f, sqrtHalf, sqrtHalf },
        Quaternion<float>{ 0.0f, 0.0f, 0.0f, 1.0f },

        Quaternion<float>{ -0.5f, 0.5f, 0.5f, 0.5f },
        Quaternion<float>{ -sqrtHalf, 0.0f, 0.0f, sqrtHalf },
        Quaternion<float>{ 0.5f, -0.5f, 0.5f, 0.5f },
        Quaternion<float>{ sqrtHalf, 0.0f, 0.0f, sqrtHalf },

        Quaternion<float>{ 0.5f, -0.5f, 0.5f, -0.5f },
        Quaternion<float>{ 0.0f, sqrtHalf, 0.0f, sqrtHalf },
        Quaternion<float>{ 0.5f, -0.5f, -0.5f, 0.5f },
        Quaternion<float>{ 0.0f, -sqrtHalf, 0.0f, sqrtHalf }
    };

    mENormal[ 0] = { -1.0f, -1.0f, 0.0f, 0.0f };
    mENormal[ 1] = { +1.0f, -1.0f, 0.0f, 0.0f };
    mENormal[ 2] = { -1.0f, +1.0f, 0.0f, 0.0f };
    mENormal[ 3] = { +1.0f, +1.0f, 0.0f, 0.0f };
    mENormal[ 4] = { -1.0f, 0.0f, -1.0f, 0.0f };
    mENormal[ 5] = { +1.0f, 0.0f, -1.0f, 0.0f };
    mENormal[ 6] = { -1.0f, 0.0f, +1.0f, 0.0f };
    mENormal[ 7] = { +1.0f, 0.0f, +1.0f, 0.0f };
    mENormal[ 8] = { 0.0f, -1.0f, -1.0f, 0.0f };
    mENormal[ 9] = { 0.0f, -1.0f, +1.0f, 0.0f };
    mENormal[10] = { 0.0f, +1.0f, -1.0f, 0.0f };
    mENormal[11] = { 0.0f, +1.0f, +1.0f, 0.0f };

    for (int i = 0; i < 12; ++i)
    {
        auto effect = std::make_shared<ConstantColorEffect>(mProgramFactory, color[i]);
        mEdgeVisual[i] = std::make_shared<Visual>(vbuffer, ibuffer, effect);
        mEdgeVisual[i]->localTransform.SetTranslation(center[i]);
        mEdgeVisual[i]->localTransform.SetRotation(orient[i]);
        mEdgeVisual[i]->localTransform.SetScale(scale[i]);
        mPVWMatrices.Subscribe(mEdgeVisual[i]->worldTransform, effect->GetPVWMatrixConstant());
        mTrackball.Attach(mEdgeVisual[i]);
    }
}

void MovingSphereBoxWindow::CreateRoundedBoxFaces()
{
    VertexFormat vformat;
    vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
    MeshFactory mf;
    mf.SetVertexFormat(vformat);
    auto visual = mf.CreateRectangle(DENSITY, DENSITY, 1.0f, 1.0f);
    auto vbuffer = visual->GetVertexBuffer();
    auto ibuffer = visual->GetIndexBuffer();

    Vector4<float> color[6] =
    {
        Vector4<float>{ 0.5f, 0.0f, 0.5f, mAlpha },
        Vector4<float>{ 0.5f, 0.0f, 0.5f, mAlpha },
        Vector4<float>{ 0.5f, 0.0f, 0.5f, mAlpha },
        Vector4<float>{ 0.5f, 0.0f, 0.5f, mAlpha },
        Vector4<float>{ 0.5f, 0.0f, 0.5f, mAlpha },
        Vector4<float>{ 0.5f, 0.0f, 0.5f, mAlpha }
    };

    Vector3<float> center[6] =
    {
        Vector3<float>{ 0.0f, 0.0f, -mBox.max[2] - mSphere.radius },
        Vector3<float>{ 0.0f, 0.0f, +mBox.max[2] + mSphere.radius },
        Vector3<float>{ 0.0f, -mBox.max[1] - mSphere.radius, 0.0f },
        Vector3<float>{ 0.0f, +mBox.max[1] + mSphere.radius, 0.0f },
        Vector3<float>{ -mBox.max[0] - mSphere.radius, 0.0f, 0.0f },
        Vector3<float>{ +mBox.max[0] + mSphere.radius, 0.0f, 0.0f }
    };

    Vector3<float> scale[6] =
    {
        Vector3<float>{ mBox.max[0], mBox.max[1], 1.0f },
        Vector3<float>{ mBox.max[0], mBox.max[1], 1.0f },
        Vector3<float>{ mBox.max[0], 1.0f, mBox.max[2] },
        Vector3<float>{ mBox.max[0], 1.0f, mBox.max[2] },
        Vector3<float>{ 1.0f, mBox.max[1], mBox.max[2] },
        Vector3<float>{ 1.0f, mBox.max[1], mBox.max[2] }
    };

    AxisAngle<3, float> aa;
    aa.axis = { 0.0f, 1.0f, 0.0f };
    aa.angle = { (float)-GTE_C_HALF_PI };
    Quaternion<float> q = Rotation<3, float>(aa);
    (void)q;

    float sqrtHalf = sqrt(0.5f); (void)sqrtHalf;
    Quaternion<float> orient[6] =
    {
        Quaternion<float>{ 1.0f, 0.0f, 0.0f, 0.0f },  // done
        Quaternion<float>{ 0.0f, 0.0f, 0.0f, 1.0f },  // done
        Quaternion<float>{ sqrtHalf, 0.0f, 0.0f, sqrtHalf },
        Quaternion<float>{ -sqrtHalf, 0.0f, 0.0f, sqrtHalf },
        Quaternion<float>{ 0.0f, -sqrtHalf, 0.0f, sqrtHalf },
        Quaternion<float>{ 0.0f, sqrtHalf, 0.0f, sqrtHalf }
    };

    mFNormal[0] = { 0.0f, 0.0f, -1.0f, 0.0f };
    mFNormal[1] = { 0.0f, 0.0f, +1.0f, 0.0f };
    mFNormal[2] = { 0.0f, -1.0f, 0.0f, 0.0f };
    mFNormal[3] = { 0.0f, +1.0f, 0.0f, 0.0f };
    mFNormal[4] = { -1.0f, 0.0f, 0.0f, 0.0f };
    mFNormal[5] = { +1.0f, 0.0f, 0.0f, 0.0f };

    for (int i = 0; i < 6; ++i)
    {
        auto effect = std::make_shared<ConstantColorEffect>(mProgramFactory, color[i]);
        mFaceVisual[i] = std::make_shared<Visual>(vbuffer, ibuffer, effect);
        mFaceVisual[i]->localTransform.SetTranslation(center[i]);
        mFaceVisual[i]->localTransform.SetRotation(orient[i]);
        mFaceVisual[i]->localTransform.SetScale(scale[i]);
        mPVWMatrices.Subscribe(mFaceVisual[i]->worldTransform, effect->GetPVWMatrixConstant());
        mTrackball.Attach(mFaceVisual[i]);
    }
}

void MovingSphereBoxWindow::CreateBox()
{
    VertexFormat vformat;
    vformat.Bind(VA_POSITION, DF_R32G32B32_FLOAT, 0);
    MeshFactory mf;
    mf.SetVertexFormat(vformat);
    mBoxVisual = mf.CreateBox(mBox.max[0], mBox.max[1], mBox.max[2]);
    Vector4<float> color{ 0.5f, 0.5f, 0.5f, mAlpha };
    auto effect = std::make_shared<ConstantColorEffect>(mProgramFactory, color);
    mBoxVisual->SetEffect(effect);
    mPVWMatrices.Subscribe(mBoxVisual->worldTransform, effect->GetPVWMatrixConstant());
    mTrackball.Attach(mBoxVisual);
}