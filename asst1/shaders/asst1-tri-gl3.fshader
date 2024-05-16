#version 150

uniform sampler2D uTex2;

in vec2 vTexCoord;
in vec4 vColor;

out vec4 fragColor;

void main(void) {
    vec4 texColor2 = texture(uTex2, vTexCoord);
    fragColor = texColor2 * vColor;
}
