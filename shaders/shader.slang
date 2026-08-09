struct VertexInput { 
  float2 position;
  float3 color;
};

struct VertexOutput {
  float4 position : SV_Position;
  float3 color;
};

[shader("vertex")]
VertexOutput vertMain(VertexInput vertexInput) {
  VertexOutput vertexOutput;
  vertexOutput.position = float4(vertexInput.position, 0.0f, 1.0f);
  vertexOutput.color = vertexInput.color;
  return vertexOutput;
}
[shader("fragment")]
float4 fragMain(VertexOutput vertexOutput) : SV_Target {
  float3 color = vertexOutput.color;
  return float4(color, 1.0);
}
