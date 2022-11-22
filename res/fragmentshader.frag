#version 330 core
out vec4 FragColor;
in vec2 texCoord;

uniform sampler2D yTexture;
uniform sampler2D uTexture;
uniform sampler2D vTexture;

void main()
{
    float y = texture(yTexture, texCoord).r;
    float u = texture(uTexture, texCoord).r - 0.5;
    float v = texture(vTexture, texCoord).r - 0.5;

    float r = y + 1.403 * v;
    float g = y - 0.343 * u - 0.714 * v;
    float b = y + 1.770 * u;

    FragColor = vec4(r, g, b, 1.0);
}
