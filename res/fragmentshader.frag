#version 330 core
out vec4 FragColor;
in vec2 texCoord;

uniform sampler2D yTexture;
uniform sampler2D uTexture;
uniform sampler2D vTexture;

void main()
{
    float y = texture(yTexture, texCoord).r - 0.063;
    float u = texture(uTexture, texCoord).r - 0.5;
    float v = texture(vTexture, texCoord).r - 0.5;

    float r = 1.164 * y + 1.596 * v;
    float g = 1.164 * y - 0.392 * u - 0.813 * v;
    float b = 1.164 * y + 2.017 * u;

    FragColor = vec4(r, g, b, 1.0);
}
