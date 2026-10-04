struct Vertex {
  float3 position;
  float4 color;
};

struct VertexOutput {
  float4 position : SV_Position;
  float4 color : COLOR0;
}

static Vertex[] vertices = {
  { { -0.5, 0.5, 0.5 }, { 1.0, 0.0, 0.0, 1.0 } },
  { { 0.5, 0.5, 0.5 }, { 0.0, 1.0, 0.0, 1.0 } },
  { { 0.0, -0.5, 0.5 }, { 0.0, 0.0, 1.0, 1.0 } },
};

[[shader("vertex")]]
VertexOutput vertex_main(uint vertex_index: SV_VertexID) {
  VertexOutput vertex;
  vertex.position = float4(vertices[vertex_index].position, 1.0);
  vertex.color = vertices[vertex_index].color;
  return vertex;
}

[[shader("fragment")]]
float4 fragment_main(VertexOutput vertex) : SV_Target { return vertex.color; }
