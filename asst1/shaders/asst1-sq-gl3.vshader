// If you have shader compilation problems, try changing this to 130
#version 150

uniform float uSquareShift;
uniform float uWratio;
uniform float uHratio;

in vec2 aPosition;
in vec2 aTexCoord;

out vec2 vTexCoord;

void main() {
   gl_Position = vec4((aPosition.x + uSquareShift) * uWratio, aPosition.y * uHratio, 0, 1);
   vTexCoord = aTexCoord;
}
