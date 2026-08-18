
struct FrameData {
  float4x4 view;
  float4x4 projection;
};

ConstantBuffer<FrameData> frameData;

struct VertexInput {
  float3 position : POSITION;
  float3 color : COLOR;
};

struct VertexOutput {
  float4 position : SV_POSITION;
  float3 color : COLOR;
};

[shader("vertex")]
VertexOutput vertMain(VertexInput vertexInput) {
  VertexOutput vertexOutput;
  vertexOutput.position = mul(
      frameData.projection,
      mul(frameData.view, float4(vertexInput.position, 1.0)));
  vertexOutput.color = vertexInput.color;
  return vertexOutput;
}

[shader("fragment")]
float4 fragMain(VertexOutput vertexOutput) : SV_TARGET {
  return float4(vertexOutput.color, 1.0);
}
