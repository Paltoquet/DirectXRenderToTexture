//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************


cbuffer ConstantBuffer : register(b0)
{
    float4 solidColor;
    float4 padding[15];
};

Texture2D t1 : register(t0);
SamplerState s1 : register(s0);

struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
};

PSInput VSMain(float4 position : POSITION, float2 uv : TEXCOORD)
{
    PSInput result;

    result.position = position;
    result.uv = uv;

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float2 pixelSize;
    t1.GetDimensions(0, pixelSize.x, pixelSize.y);

    return t1.Sample(s1, input.uv);
    //return float4(input.uv.x, input.uv.y, 0.0, 1.0); //return solidColor;
    //return input.color;
}
