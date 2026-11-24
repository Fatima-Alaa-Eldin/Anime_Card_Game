#version 330 core

out vec4 FragColor;

in VS_OUT
{
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoords;
} fs_in;

// Set per mesh by Model::Draw from the imported MTL material (Kd).
uniform vec3 objectColor;
uniform vec3 lightPos;
uniform vec3 viewPos;

void main()
{
    vec3 normal = normalize(fs_in.Normal);
    vec3 lightDirection = normalize(lightPos - fs_in.FragPos);
    vec3 viewDirection = normalize(viewPos - fs_in.FragPos);

    // Neutral ambient + diffuse preserve the MTL material color.
    vec3 ambient = 0.42 * objectColor;
    float diffuseStrength = max(dot(normal, lightDirection), 0.0);
    vec3 diffuse = 0.82 * diffuseStrength * objectColor;

    // White highlights make the piano's black lacquer look polished,
    // without tinting its base material brown.
    vec3 reflectDirection = reflect(-lightDirection, normal);
    float specularStrength = pow(max(dot(viewDirection, reflectDirection), 0.0), 56.0);
    vec3 specular = 0.48 * specularStrength * vec3(1.0);

    FragColor = vec4(ambient + diffuse + specular, 1.0);
}
