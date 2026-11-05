#version 150

// Scale must be uniform, because the normals only go through the world matrix.

uniform mat4 worldViewProj;
uniform mat4 world;

in vec4 vertex;
in vec3 normal;
in vec2 uv0;

out vec3 worldPos;
out vec3 worldNormal;
out vec2 uv;

void main( )
{
  worldPos = ( world * vertex ).xyz;
  worldNormal = mat3( world ) * normal;
  uv = uv0;
  gl_Position = worldViewProj * vertex;
}
