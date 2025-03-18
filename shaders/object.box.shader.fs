#version 460 core
in vec3 Normal;
in vec3 FragPos;
in vec2 TexCoords;

out vec4 FragColor;

struct Material
{
    sampler2D diffuse;
    sampler2D specular;
    float shininess;
};

struct Light
{
    vec3 position;
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
    float constant;
    float linear;
    float quadratic;
    float cutOff;
};

uniform Material material;
uniform Light light;

void main()
{
    vec3 lightDir = normalize(- FragPos);
    
    // check if lighting is inside the spotlight cone
    float theta = dot(lightDir, normalize(-light.direction));

    if(theta > light.cutOff)
    {
        // Ambient
        vec3 ambient = light.ambient * texture(material.diffuse, TexCoords).rgb;

        // Diffuse
        vec3 norm = normalize(Normal);
        float diff = max(dot(norm, lightDir), 0.0);
        vec3 diffuse = light.diffuse * diff * texture(material.diffuse, TexCoords).rgb;

        // Specular
        vec3 viewDir = normalize(- FragPos);
        vec3 reflectDir = reflect(-lightDir, norm);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), material.shininess);
        vec3 specular = light.specular * spec * texture(material.specular, TexCoords).rgb;
        /*
        float distance    = length(- FragPos);
        float attenuation = 1.0 / (light.constant + light.linear * distance + 
                        light.quadratic * (distance * distance));
        ambient  *= attenuation; 
        diffuse  *= attenuation;
        specular *= attenuation;  
        */
        vec3 result = ambient + diffuse + specular;
        FragColor = vec4(result, 1.0);
    }
    else
    {
        FragColor = vec4(light.ambient * texture(material.diffuse, TexCoords).rgb, 1.0);
    }
}