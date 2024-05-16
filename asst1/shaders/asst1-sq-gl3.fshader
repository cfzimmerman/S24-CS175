// If you have shader compilation problems, try changing this to 130
#version 150

uniform float uBlender;
uniform sampler2D uTex0, uTex1;

in vec2 vTexCoord;

out vec4 fragColor;

void main(void) {

    // The texture(..) calls always return a vec4. Data comes out of a texture
    // in RGBA format
    vec4 texColor0 = texture(uTex0, vTexCoord);
    vec4 texColor1 = texture(uTex1, vTexCoord);

    // some fancy blending
    float lerper = clamp(.5 * uBlender, 0., 1.);


    // fragColor is a vec4. The components are interpreted as red, green, blue,
    // and alpha
    
    fragColor = lerper*texColor0+(1.-lerper)*texColor1;
}
