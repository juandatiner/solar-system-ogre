#version 150

// Rings of Saturn: not lit by diffuse lighting, but darkened by the planet.

in vec3 worldPos;
in vec3 worldNormal;
in vec2 uv;

out vec4 fragColour;

uniform sampler2D diffuseMap;
uniform vec4 lightPos;
uniform vec4 planet;      // xyz: center; w: radius

void main( )
{
  vec3 L = normalize( lightPos.xyz - worldPos );
  vec3 v = worldPos - planet.xyz;

  // The shadow is a cylinder on the side opposite to the Sun
  float shadow = 1.0;
  if( dot( v, L ) < 0.0 )
    shadow = smoothstep( 0.97 * planet.w, 1.03 * planet.w, length( cross( v, L ) ) );

  vec4 colour = texture( diffuseMap, uv );
  fragColour = vec4( colour.rgb * mix( 0.06, 1.0, shadow ), colour.a );
}
