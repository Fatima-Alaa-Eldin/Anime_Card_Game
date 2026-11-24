#version 330 core
out vec4 FragColor;

in vec2 TexCoord;

uniform vec3 objectColor;
uniform sampler2D texture1;
uniform bool useTexture;
uniform bool isCloak;

void main()
{
    if (useTexture)
    {
        vec4 texColor = texture(texture1, TexCoord);
        if (texColor.a < 0.1)
            discard;
        if (isCloak && texColor.r > 0.50 && texColor.g > 0.50 && texColor.b > 0.50)
            discard;
        FragColor = texColor * vec4(objectColor, 1.0);
    }
    else
    {
        FragColor = vec4(objectColor, 1.0);
    }
}