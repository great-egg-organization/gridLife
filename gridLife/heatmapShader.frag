#version 450

out vec4 outColor;

layout(origin_upper_left) in vec4 gl_FragCoord;

uniform vec2 offset;
uniform vec2 winDim;
uniform ivec2 gridDim;

readonly buffer SSBO{
	float heatmap[];
};

vec3 pal( in float t, in vec3 a, in vec3 b, in vec3 c, in vec3 d ) 
{
	return a + b*cos( 6.28318*(c*t+d) );
}

vec3 myPalette(float t){ return pal(t, vec3(0.52, 0.52, 0.5), vec3(1, 1, 0), vec3(0.3, 0.3, 0.), vec3(0.63, 0.52, 0)); }

void main() {
	ivec2 gridPos = ivec2((gl_FragCoord.x + offset.x)/winDim.x, (gl_FragCoord.y + offset.y)/winDim.y);
	uint ind = gridPos.x + gridPos.y * gridDim.x;
  
	float val = (heatmap[ind]) / 800.0f;
	outColor = vec4(myPalette(val), 0.8f);
}
