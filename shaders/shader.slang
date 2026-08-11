struct UniformBufferObject {
  float4x4 model;
  float4x4 view;
  float4x4 projection;
};

ConstantBuffer<UniformBufferObject> ubo;

struct VertexInput {
  float2 position;
  float3 color;
  float2 texCoord;
};

struct VertexOutput {
  float4 position : SV_Position;
  float3 color;
  float2 texCoord;
};

Sampler2D texture;

[shader("vertex")]
VertexOutput vertMain(VertexInput vertexInput) {
  VertexOutput vertexOutput;
  vertexOutput.position = mul(
      ubo.projection,
      mul(ubo.view, mul(ubo.model, float4(vertexInput.position, 0.0f, 1.0f))));
  vertexOutput.color = vertexInput.color;
  vertexOutput.texCoord = vertexInput.texCoord;
  return vertexOutput;
}
[shader("fragment")]
float4 fragMain(VertexOutput vertexOutput) : SV_Target {
  return texture.Sample(vertexOutput.texCoord);
}
