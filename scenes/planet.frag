#version 150

// Lambert diffuse lighting, from a point light. Variants:
// - EARTH: city lights on the night side, and the shadow of a solar eclipse.
// - SATURN: shadow of the rings on the planet.
// - CLOUDS: transparent layer, where brightness of the texture gives opacity.

in vec3 worldPos;
in vec3 worldNormal;
in vec2 uv;

out vec4 fragColour;

uniform sampler2D diffuseMap;
uniform vec4 lightPos;
uniform vec4 ambient;

#ifndef CLOUDS
uniform vec4 cameraPos;
uniform vec4 tint;
uniform vec4 atmosphere;  // rgb: colour; a: strength
#endif

#ifdef EARTH
uniform sampler2D nightMap;
uniform vec4 spot;        // xyz: center of the shadow; w: radius, 0 when absent
#endif

#ifdef SATURN
uniform vec4 ringCenter;
uniform vec4 ringNormal;
uniform vec4 ringRadii;   // x: inner; y: outer
#endif

void main( )
{
  vec3 N = normalize( worldNormal );
  vec3 L = normalize( lightPos.xyz - worldPos );
  float diffuse = max( dot( N, L ), 0.0 );
  vec3 colour = texture( diffuseMap, uv ).rgb;

#ifdef CLOUDS
  float opacity = dot( colour, vec3( 1.0 / 3.0 ) );
  fragColour = vec4( ambient.rgb + vec3( diffuse ), 0.9 * opacity );
#else
  float light = 1.0;

#ifdef EARTH
  if( spot.w > 0.0 )
  {
    // Distance from this point to the shadow axis, which goes towards the Sun
    float d = length( cross( worldPos - spot.xyz, L ) );
    light = mix( 0.05, 1.0, smoothstep( 0.05 * spot.w, spot.w, d ) );
  }
#endif

#ifdef SATURN
  // Walk towards the Sun until the plane of the rings: is that point inside them?
  float facing = dot( L, ringNormal.xyz );
  if( abs( facing ) > 0.001 )
  {
    float s = dot( ringCenter.xyz - worldPos, ringNormal.xyz ) / facing;
    if( s > 0.0 )
    {
      float r = length( worldPos + ( L * s ) - ringCenter.xyz );
      light *=
        1.0 -
        0.55 * smoothstep( ringRadii.x, ringRadii.x + 0.3, r )
        * ( 1.0 - smoothstep( ringRadii.y - 0.3, ringRadii.y, r ) );
    }
  }
#endif

  vec3 result = colour * tint.rgb * ( ambient.rgb + ( diffuse * light ) );

#ifdef EARTH
  float night = 1.0 - smoothstep( 0.0, 0.25, dot( N, L ) );
  result += texture( nightMap, uv ).rgb * night;
#endif

  // Atmosphere: glows at the edge of the planet, only on the sunlit side
  vec3 V = normalize( cameraPos.xyz - worldPos );
  float rim = pow( 1.0 - max( dot( N, V ), 0.0 ), 3.0 );
  result +=
    atmosphere.rgb * atmosphere.a * rim
    * smoothstep( -0.1, 0.4, dot( N, L ) ) * light;

  fragColour = vec4( result, 1.0 );
#endif
}
