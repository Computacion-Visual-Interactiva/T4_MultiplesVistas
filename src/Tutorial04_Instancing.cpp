/*
 *  Copyright 2019-2024 Diligent Graphics LLC
 *  Copyright 2015-2019 Egor Yusov
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *  In no event and under no legal theory, whether in tort (including negligence),
 *  contract, or otherwise, unless required by applicable law (such as deliberate
 *  and grossly negligent acts) or agreed to in writing, shall any Contributor be
 *  liable for any damages, including any direct, indirect, special, incidental,
 *  or consequential damages of any character arising as a result of this License or
 *  out of the use or inability to use the software (including but not limited to damages
 *  for loss of goodwill, work stoppage, computer failure or malfunction, or any and
 *  all other commercial damages or losses), even if such Contributor has been advised
 *  of the possibility of such damages.
 */

#include <random>

#include "Tutorial04_Instancing.hpp"
#include "MapHelper.hpp"
#include "GraphicsUtilities.h"
#include "TextureUtilities.h"
#include "ColorConversion.h"
#include "../../Common/src/TexturedCube.hpp"
#include "imgui.h"

namespace Diligent
{

SampleBase* CreateSample()
{
    return new Tutorial04_Instancing();
}

void Tutorial04_Instancing::CreatePipelineState()
{
    // clang-format off
    // Define vertex shader input layout
    // This tutorial uses two types of input: per-vertex data and per-instance data.
    LayoutElement LayoutElems[] =
    {
        // Per-vertex data - first buffer slot
        // Attribute 0 - vertex position
        LayoutElement{0, 0, 3, VT_FLOAT32, False},
        // Attribute 1 - texture coordinates
        LayoutElement{1, 0, 2, VT_FLOAT32, False},
            
        // Per-instance data - second buffer slot
        // We will use four attributes to encode instance-specific 4x4 transformation matrix
        // Attribute 2 - first row
        LayoutElement{2, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
        // Attribute 3 - second row
        LayoutElement{3, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
        // Attribute 4 - third row
        LayoutElement{4, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE},
        // Attribute 5 - fourth row
        LayoutElement{5, 1, 4, VT_FLOAT32, False, INPUT_ELEMENT_FREQUENCY_PER_INSTANCE}
    };
    // clang-format on

    // Create a shader source stream factory to load shaders from files.
    RefCntAutoPtr<IShaderSourceInputStreamFactory> pShaderSourceFactory;
    m_pEngineFactory->CreateDefaultShaderSourceStreamFactory(nullptr, &pShaderSourceFactory);

    TexturedCube::CreatePSOInfo CubePsoCI;
    CubePsoCI.pDevice                = m_pDevice;
    CubePsoCI.RTVFormat              = m_pSwapChain->GetDesc().ColorBufferFormat;
    CubePsoCI.DSVFormat              = m_pSwapChain->GetDesc().DepthBufferFormat;
    CubePsoCI.pShaderSourceFactory   = pShaderSourceFactory;
    CubePsoCI.VSFilePath             = "cube_inst.vsh";
    CubePsoCI.PSFilePath             = "cube_inst.psh";
    CubePsoCI.ExtraLayoutElements    = LayoutElems;
    CubePsoCI.NumExtraLayoutElements = _countof(LayoutElems);

    m_pPSO = TexturedCube::CreatePipelineState(CubePsoCI, m_ConvertPSOutputToGamma);

    // Create dynamic uniform buffer that will store our transformation matrix
    // Dynamic buffers can be frequently updated by the CPU
    CreateUniformBuffer(m_pDevice, sizeof(float4x4) * 2, "VS constants CB", &m_VSConstants);

    // Since we did not explicitly specify the type for 'Constants' variable, default
    // type (SHADER_RESOURCE_VARIABLE_TYPE_STATIC) will be used. Static variables
    // never change and are bound directly to the pipeline state object.
    m_pPSO->GetStaticVariableByName(SHADER_TYPE_VERTEX, "Constants")->Set(m_VSConstants);

    // Since we are using mutable variable, we must create a shader resource binding object
    // http://diligentgraphics.com/2016/03/23/resource-binding-model-in-diligent-engine-2-0/
    m_pPSO->CreateShaderResourceBinding(&m_SRB, true);
}

void Tutorial04_Instancing::CreateInstanceBuffer()
{
    // Create instance data buffer that will store transformation matrices
    BufferDesc InstBuffDesc;
    InstBuffDesc.Name = "Instance data buffer";
    // Use default usage as this buffer will only be updated when grid size changes
    InstBuffDesc.Usage     = USAGE_DEFAULT;
    InstBuffDesc.BindFlags = BIND_VERTEX_BUFFER;
    InstBuffDesc.Size      = sizeof(float4x4) * MaxInstances;
    m_pDevice->CreateBuffer(InstBuffDesc, nullptr, &m_InstanceBuffer);
    PopulateInstanceBuffer();
}

void Tutorial04_Instancing::UpdateUI()
{
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        if (ImGui::SliderInt("Grid Size", &m_GridSize, 32, 32))
        {
            PopulateInstanceBuffer();
        }
        ImGui::Text("Camera View");
        ImGui::RadioButton("Default", &m_CameraMode, 0);
        ImGui::RadioButton("Front", &m_CameraMode, 1);
        ImGui::RadioButton("Top", &m_CameraMode, 2);
        ImGui::RadioButton("Side", &m_CameraMode, 3);
        ImGui::RadioButton("Bottom", &m_CameraMode, 4);
    }
    ImGui::End();
}


void Tutorial04_Instancing::Initialize(const SampleInitInfo& InitInfo)
{
    SampleBase::Initialize(InitInfo);

    CreatePipelineState();

    // Load textured cube
    m_CubeVertexBuffer = TexturedCube::CreateVertexBuffer(m_pDevice, GEOMETRY_PRIMITIVE_VERTEX_FLAG_POS_TEX);
    m_CubeIndexBuffer  = TexturedCube::CreateIndexBuffer(m_pDevice);
    m_TextureSRV       = TexturedCube::LoadTexture(m_pDevice, "DGLogo.png")->GetDefaultView(TEXTURE_VIEW_SHADER_RESOURCE);
    // Set cube texture SRV in the SRB
    m_SRB->GetVariableByName(SHADER_TYPE_PIXEL, "g_Texture")->Set(m_TextureSRV);

    CreateInstanceBuffer();
}

static float angle = PI_F / 4;

void Tutorial04_Instancing::PopulateInstanceBuffer()
{
    // Populate instance data buffer
    const auto            zGridSize = static_cast<size_t>(m_GridSize);
    std::vector<float4x4> InstanceData(zGridSize * zGridSize * zGridSize);

    float fGridSize = static_cast<float>(m_GridSize);

    std::mt19937 gen; // Standard mersenne_twister_engine. Use default seed
                      // to generate consistent distribution.

    std::uniform_real_distribution<float> scale_distr(0.3f, 1.0f);
    std::uniform_real_distribution<float> offset_distr(-0.15f, +0.15f);
    std::uniform_real_distribution<float> rot_distr(-PI_F, +PI_F);

    float BaseScale = 0.6f / fGridSize;
    int   instId    = 0;

    // Figuras en el Mobil

    angle += 0.01;

    float4x4 matrix        = float4x4::Scale(0.7, 0.7, 0.7) * float4x4::Translation(0, 4, 0) * float4x4::RotationY(angle); // Center lv 1
    InstanceData[instId++] = matrix;

    matrix                 = float4x4::Scale(0.6, 0.6, 0.6) * float4x4::Translation(6, 6, 0) * float4x4::RotationY(angle); // Right lv 1
    InstanceData[instId++] = matrix;

    matrix                 = float4x4::Scale(0.6, 0.6, 0.6) * float4x4::Translation(-6, 6, 0) * float4x4::RotationY(angle); // Left lv 1
    InstanceData[instId++] = matrix;

    matrix                 = float4x4::Scale(0.6, 0.6, 0.6) * float4x4::Translation(0, 6, 6) * float4x4::RotationY(angle); // Front lv 1
    InstanceData[instId++] = matrix;

    matrix                 = float4x4::Scale(0.6, 0.6, 0.6) * float4x4::Translation(0, 6, -6) * float4x4::RotationY(angle); // Back lv 1
    InstanceData[instId++] = matrix;

    matrix                 = float4x4::Scale(0.5, 0.5, 0.5) * float4x4::Translation(6, 3, 0) * float4x4::RotationY(angle); // Right lv 2
    InstanceData[instId++] = matrix;

    matrix                 = float4x4::Scale(0.5, 0.5, 0.5) * float4x4::Translation(-6, 3, 0) * float4x4::RotationY(angle); // Left lv 2
    InstanceData[instId++] = matrix;

    matrix                 = float4x4::Scale(0.5, 0.5, 0.5) * float4x4::Translation(0, 3, 6) * float4x4::RotationY(angle); // Front lv 2
    InstanceData[instId++] = matrix;

    matrix                 = float4x4::Scale(0.5, 0.5, 0.5) * float4x4::Translation(0, 3, -6) * float4x4::RotationY(angle); // Back lv 2
    InstanceData[instId++] = matrix;

    matrix                 = float4x4::Scale(0.5, 0.5, 0.5) * float4x4::Translation(6, 0, 0) * float4x4::RotationY(angle); // Right lv 3
    InstanceData[instId++] = matrix;

    matrix                 = float4x4::Scale(0.5, 0.5, 0.5) * float4x4::Translation(-6, 0, 0) * float4x4::RotationY(angle); // Left lv 3
    InstanceData[instId++] = matrix;

    matrix                 = float4x4::Scale(0.5, 0.5, 0.5) * float4x4::Translation(0, 0, 6) * float4x4::RotationY(angle); // Front lv 3
    InstanceData[instId++] = matrix;

    matrix                 = float4x4::Scale(0.5, 0.5, 0.5) * float4x4::Translation(0, 0, -6) * float4x4::RotationY(angle); // Back lv 3
    InstanceData[instId++] = matrix;


    // Tubos

    matrix = float4x4::Scale(6, 0.08, 0.08) * float4x4::Translation(0, 8, 0) * float4x4::RotationY(angle); // Center lv 1
    InstanceData[instId++] = matrix;

    matrix                 = float4x4::Scale(0.08, 0.08, 6) * float4x4::Translation(0, 8, 0) * float4x4::RotationY(angle); // Center lv 1
    InstanceData[instId++] = matrix;

    //Palos para abajos

    matrix                 = float4x4::Scale(0.08, 2, 0.08) * float4x4::Translation(0, 6, 0) * float4x4::RotationY(angle);
    InstanceData[instId++] = matrix;

    matrix                 = float4x4::Scale(0.08, 4, 0.08) * float4x4::Translation(6, 4, 0) * float4x4::RotationY(angle);
    InstanceData[instId++] = matrix;

    matrix                 = float4x4::Scale(0.08, 4, 0.08) * float4x4::Translation(0, 4, 6) * float4x4::RotationY(angle);
    InstanceData[instId++] = matrix;

    matrix                 = float4x4::Scale(0.08, 4, 0.08) * float4x4::Translation(-6, 4, 0) * float4x4::RotationY(angle);
    InstanceData[instId++] = matrix;

    matrix                 = float4x4::Scale(0.08, 4, 0.08) * float4x4::Translation(0, 4, -6) * float4x4::RotationY(angle);
    InstanceData[instId++] = matrix;

    // Update instance data buffer
    Uint32 DataSize = static_cast<Uint32>(sizeof(InstanceData[0]) * InstanceData.size());
    m_pImmediateContext->UpdateBuffer(m_InstanceBuffer, 0, DataSize, InstanceData.data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
}


// Render a frame
void Tutorial04_Instancing::Render()
{
    auto* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
    auto* pDSV = m_pSwapChain->GetDepthBufferDSV();
    // Clear the back buffer
    float4 ClearColor = {0.350f, 0.350f, 0.350f, 1.0f};
    if (m_ConvertPSOutputToGamma)
    {
        // If manual gamma correction is required, we need to clear the render target with sRGB color
        ClearColor = LinearToSRGB(ClearColor);
    }
    m_pImmediateContext->ClearRenderTarget(pRTV, ClearColor.Data(), RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pImmediateContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    {
        // Map the buffer and write current world-view-projection matrix
        MapHelper<float4x4> CBConstants(m_pImmediateContext, m_VSConstants, MAP_WRITE, MAP_FLAG_DISCARD);
        CBConstants[0] = m_ViewProjMatrix;
        CBConstants[1] = m_RotationMatrix;
    }

    // Bind vertex, instance and index buffers
    const Uint64 offsets[] = {0, 0};
    IBuffer*     pBuffs[]  = {m_CubeVertexBuffer, m_InstanceBuffer};
    m_pImmediateContext->SetVertexBuffers(0, _countof(pBuffs), pBuffs, offsets, RESOURCE_STATE_TRANSITION_MODE_TRANSITION, SET_VERTEX_BUFFERS_FLAG_RESET);
    m_pImmediateContext->SetIndexBuffer(m_CubeIndexBuffer, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Set the pipeline state
    m_pImmediateContext->SetPipelineState(m_pPSO);
    // Commit shader resources. RESOURCE_STATE_TRANSITION_MODE_TRANSITION mode
    // makes sure that resources are transitioned to required states.
    m_pImmediateContext->CommitShaderResources(m_SRB, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    DrawIndexedAttribs DrawAttrs;       // This is an indexed draw call
    DrawAttrs.IndexType    = VT_UINT32; // Index type
    DrawAttrs.NumIndices   = 36;
    DrawAttrs.NumInstances = m_GridSize * m_GridSize * m_GridSize; // The number of instances
    // Verify the state of vertex and index buffers
    DrawAttrs.Flags = DRAW_FLAG_VERIFY_ALL;
    m_pImmediateContext->DrawIndexed(DrawAttrs);
}

void Tutorial04_Instancing::Update(double CurrTime, double ElapsedTime)
{
    SampleBase::Update(CurrTime, ElapsedTime);
    UpdateUI();
    PopulateInstanceBuffer();

    float4x4 View;

    switch (m_CameraMode)
    {
        default:
        case 0:
            View = float4x4::RotationX(-0.3f) * float4x4::Translation(0.f, 0.f, 40.0f);
            break;

        case 1:
            View = float4x4::Translation(0.f, 0.f, 40.f);
            break;

        case 2:
            View = float4x4::RotationX(-PI_F/2.0f) * float4x4::Translation(0.f, 0.f, 40.f);
            break;

        case 3:
            View = float4x4::RotationY(PI_F/2.0f) * float4x4::Translation(0.f, 0.f, 40.f);
            break;

        case 4:
            View = float4x4::RotationX(PI_F / 2.0f) * float4x4::Translation(0.f, 0.f, 40.f);
            break;
    }

    // Pretransform de la superficie (por si la plataforma gira la pantalla)
    auto SrfPreTransform = GetSurfacePretransformMatrix(float3{0, 0, 1});

    // Proyección ajustada a la ventana
    auto Proj = GetAdjustedProjectionMatrix(PI_F / 4.0f, 0.1f, 100.f);

    // Multiplicamos todo
    m_ViewProjMatrix = View * SrfPreTransform * Proj;

    // Rotación global (si la deseas). Aquí la dejamos en 0
    m_RotationMatrix = float4x4::RotationY(static_cast<float>(CurrTime) * 0.f) *
        float4x4::RotationX(static_cast<float>(CurrTime) * 0.f);
}

} // namespace Diligent