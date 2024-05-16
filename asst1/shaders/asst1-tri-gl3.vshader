#version 150

uniform float uTriangleShift;

uniform float uTriDeltaX;
uniform float uTriDeltaY;
uniform float uWratio;
uniform float uHratio;

in vec2 aPosition;
in vec2 aTexCoord;
in vec4 aColor;

out vec2 vTexCoord;
out vec4 vColor;

void main() {
   gl_Position = vec4(
      (aPosition.x + uTriDeltaX) * uWratio, 
      (aPosition.y + uTriDeltaY) * uHratio, 
      0, 1
    );
   vTexCoord = aTexCoord;
   vColor = aColor;
}
